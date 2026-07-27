module;

#include <string>
#include <filesystem>

export module Data.CsvDataSource;

import Data.CsvParser;
import Data.DataTable;
import Data.DataSource;
import Core.ArtifactString;

export namespace ArtifactCore {

class CsvDataSource : public IDataSource {
public:
    explicit CsvDataSource(const String& uri)
        : uri_(uri)
        , path_(toStdString(uri))
    {
        load_();
    }

    explicit CsvDataSource(const std::filesystem::path& path)
        : uri_(path.string())
        , path_(path)
    {
        load_();
    }

    DataSourceInfo info() const override {
        DataSourceInfo inf;
        inf.uri = uri_;
        inf.displayName = path_.filename().string();
        inf.kind = DataSourceKind::Csv;
        inf.rowCount = table_.rowCount();
        inf.columnCount = table_.columnCount();

        if (std::filesystem::exists(path_)) {
            inf.fileSize = static_cast<int64_t>(std::filesystem::file_size(path_));
            inf.lastModified = static_cast<int64_t>(
                std::filesystem::last_write_time(path_).time_since_epoch().count());
        }

        return inf;
    }

    const DataTable& table() const override { return table_; }

    bool reload() override {
        return load_();
    }

    bool isValid() const override { return lastError_.isEmpty(); }

    String lastError() const override { return lastError_; }

private:
    bool load_() {
        CsvParseOptions opts;
        opts.delimiter = CsvDelimiter::Auto;
        opts.hasHeader = true;
        opts.trimWhitespace = true;
        opts.skipEmptyRows = true;

        auto result = CsvParser::parseFile(path_, opts);
        if (!result.ok()) {
            lastError_ = result.errorMessage;
            table_ = DataTable();
            return false;
        }

        table_ = DataTable(result);
        lastError_.clear();
        return true;
    }

    String uri_;
    std::filesystem::path path_;
    DataTable table_;
    String lastError_;
};

inline DataSourcePtr makeCsvDataSource(const String& uri) {
    return makeShared<CsvDataSource>(uri);
}

struct CsvDataSourceAutoRegister {
    CsvDataSourceAutoRegister() {
        DataSourceRegistry::instance().registerFormat(".csv", makeCsvDataSource);
        DataSourceRegistry::instance().registerFormat(".tsv", [](const String& uri) {
            CsvParseOptions opts;
            opts.delimiter = CsvDelimiter::Tab;
            auto result = CsvParser::parseFile(toStdString(uri), opts);
            if (!result.ok()) return DataSourcePtr{};
            return makeShared<CsvDataSource>(uri);
        });
    }
};

inline CsvDataSourceAutoRegister& csvDataSourceAutoRegister() {
    static CsvDataSourceAutoRegister reg;
    return reg;
}

} // namespace ArtifactCore
