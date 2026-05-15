module;

#include <string>
#include <memory>
#include <filesystem>
#include <vector>

export module Asset.DataAssetFile;

import Asset.File;
import AssetType;
import Utils.String.UniString;
import Data.CsvParser;
import Data.DataTable;
import Data.ColumnType;
import Data.TypeInference;

export namespace ArtifactCore {

class DataAssetFile : public AbstractAssetFile {
public:
    DataAssetFile() = default;

    AssetType assetType() const { return AssetType::Data; }

protected:
    bool _load() override {
        const std::filesystem::path path = std::string(filePath());
        if (path.empty()) return false;

        CsvParseOptions opts;
        opts.delimiter = CsvDelimiter::Auto;
        opts.hasHeader = true;
        opts.trimWhitespace = true;
        opts.skipEmptyRows = true;

        auto result = CsvParser::parseFile(path, opts);
        if (!result.ok()) {
            return false;
        }

        table_ = DataTable(result);

        meta().setValue("rowCount", std::to_string(table_.rowCount()));
        meta().setValue("columnCount", std::to_string(table_.columnCount()));

        std::string schema;
        for (int c = 0; c < table_.columnCount(); ++c) {
            if (c > 0) schema += ", ";
            std::vector<std::string> colValues;
            colValues.reserve(table_.rowCount());
            for (int r = 0; r < table_.rowCount(); ++r) {
                colValues.push_back(table_.getString(r, c));
            }
            auto inferredType = TypeInference::inferFromValues(colValues);
            schema += table_.columnName(c) + ":" + columnTypeToString(inferredType);
        }
        meta().setValue("schema", schema);

        return true;
    }

    void _unload() override {
        table_.clear();
    }

    const DataTable& table() const { return table_; }

private:
    DataTable table_;
};

} // namespace ArtifactCore
