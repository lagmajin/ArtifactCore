module;

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <functional>

export module Data.DataSource;

import Data.DataTable;
import Data.ColumnType;

export namespace ArtifactCore {

enum class DataSourceKind {
    Unknown,
    Csv,
    Json,
    Tsv
};

inline const char* dataSourceKindToString(DataSourceKind k) {
    switch (k) {
    case DataSourceKind::Unknown: return "Unknown";
    case DataSourceKind::Csv:     return "Csv";
    case DataSourceKind::Json:    return "Json";
    case DataSourceKind::Tsv:     return "Tsv";
    }
    return "Unknown";
}

struct DataSourceInfo {
    std::string uri;
    std::string displayName;
    DataSourceKind kind = DataSourceKind::Unknown;
    int64_t fileSize = 0;
    int64_t lastModified = 0;
    int rowCount = 0;
    int columnCount = 0;
};

class IDataSource {
public:
    virtual ~IDataSource() = default;

    virtual DataSourceInfo info() const = 0;
    virtual const DataTable& table() const = 0;
    virtual bool reload() = 0;
    virtual bool isValid() const = 0;
    virtual std::string lastError() const = 0;
};

using DataSourcePtr = std::shared_ptr<IDataSource>;
using DataSourceFactory = std::function<DataSourcePtr(const std::string& uri)>;

class DataSourceRegistry {
public:
    static DataSourceRegistry& instance() {
        static DataSourceRegistry inst;
        return inst;
    }

    void registerFormat(const std::string& extension, DataSourceFactory factory) {
        factories_[extension] = std::move(factory);
    }

    DataSourcePtr open(const std::string& uri) {
        std::string ext = extensionFromUri(uri);
        auto it = factories_.find(ext);
        if (it != factories_.end()) {
            return it->second(uri);
        }
        return nullptr;
    }

    DataSourcePtr open(const std::filesystem::path& path) {
        return open(path.string());
    }

    std::vector<std::string> registeredExtensions() const {
        std::vector<std::string> exts;
        exts.reserve(factories_.size());
        for (const auto& [ext, _] : factories_) {
            exts.push_back(ext);
        }
        return exts;
    }

private:
    static std::string extensionFromUri(const std::string& uri) {
        std::string ext;
        size_t dotPos = uri.find_last_of('.');
        if (dotPos != std::string::npos) {
            ext = uri.substr(dotPos);
        }
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return ext;
    }

    std::unordered_map<std::string, DataSourceFactory> factories_;
};

} // namespace ArtifactCore
