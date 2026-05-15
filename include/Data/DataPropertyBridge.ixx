module;

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

export module Data.DataPropertyBridge;

import Data.DataTable;
import Data.DataSource;
import Data.CsvDataSource;
import Data.DataCache;
import Data.FileWatcher;
import Property;
import Property.Abstract;
import Property.Group;
import Property.Path;

export namespace ArtifactCore {

class DataPropertyBridge {
public:
    static void registerDataSource(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& cache = DataCache::instance();

        DataSourcePtr source = cache.get(uri);
        if (!source) {
            source = DataSourceRegistry::instance().open(uri);
            if (!source) return;

            int64_t fileMod = 0;
            auto info = source->info();
            fileMod = info.lastModified;

            cache.put(uri, source, fileMod);

            FileWatcher::instance().watch(uri, [uri](const std::string&) {
                DataCache::instance().invalidate(uri);
            });
        }

        const auto& table = source->table();
        std::string ownerPath = "data." + sanitizePath(uri);
        const QString ownerPathQ = QString::fromStdString(ownerPath);

        auto group = std::make_shared<PropertyGroup>();
        group->setName(QString::fromStdString(uri));

        auto path = PropertyPath(ownerPathQ);

        for (int c = 0; c < table.columnCount(); ++c) {
            auto colName = table.columnName(c);
            auto prop = std::make_shared<AbstractProperty>();
            const QString colNameQ = QString::fromStdString(colName);
            prop->setName(colNameQ);
            prop->setType(PropertyType::String);
            prop->setDisplayLabel(colNameQ);

            std::string preview;
            for (int r = 0; r < std::min(table.rowCount(), 3); ++r) {
                if (r > 0) preview += ", ";
                preview += table.getString(r, c);
            }
            if (table.rowCount() > 3) preview += "...";
            prop->setValue(QString::fromStdString(preview));

            group->addProperty(prop);
        }

        globalPropertyRegistry().registerOwnerSnapshot(ownerPathQ, QString::fromStdString(uri), QString("Data"), *group);
        registeredSources_[uri] = source;
    }

    static void unregisterSource(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string ownerPath = "data." + sanitizePath(uri);
        globalPropertyRegistry().unregisterOwner(QString::fromStdString(ownerPath));
        registeredSources_.erase(uri);
        DataCache::instance().invalidate(uri);
        FileWatcher::instance().unwatch(uri);
    }

    static std::vector<std::string> registeredUris() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> uris;
        uris.reserve(registeredSources_.size());
        for (const auto& [uri, _] : registeredSources_) {
            uris.push_back(uri);
        }
        return uris;
    }

    static std::string getValue(const std::string& uri, int row, const std::string& column) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredSources_.find(uri);
        if (it == registeredSources_.end()) return "";
        return it->second->table().getString(row, column);
    }

    static int getRowCount(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredSources_.find(uri);
        if (it == registeredSources_.end()) return 0;
        return it->second->table().rowCount();
    }

    static int getColumnCount(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredSources_.find(uri);
        if (it == registeredSources_.end()) return 0;
        return it->second->table().columnCount();
    }

private:
    static std::string sanitizePath(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            if (c == '/' || c == '\\') result += '.';
            else if (c == ' ') result += '_';
            else if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') result += c;
        }
        return result;
    }

    static std::unordered_map<std::string, DataSourcePtr> registeredSources_;
    static std::mutex mutex_;
};

inline std::unordered_map<std::string, DataSourcePtr> DataPropertyBridge::registeredSources_;
inline std::mutex DataPropertyBridge::mutex_;

} // namespace ArtifactCore
