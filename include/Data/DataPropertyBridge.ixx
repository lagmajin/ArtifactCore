module;

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <QString>

export module Data.DataPropertyBridge;

import Container.NamedVector;

import Core.ArtifactString;
import Data.DataTable;
import Data.DataSource;
import Data.CsvDataSource;
import Data.DataCache;
import Data.FileWatcher;
import Property;
import Property.Abstract;
import Property.Group;
import Property.Path;
import Memory.SharedPtr;

export namespace ArtifactCore {

class DataPropertyBridge {
public:
    static void registerDataSource(const String& uri) {
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

            FileWatcher::instance().watch(uri, [uri](const String&) {
                DataCache::instance().invalidate(uri);
            });
        }

        const auto& table = source->table();
        ZeroString ownerPath = ZeroString("data.") + sanitizePath(uri);
        const QString ownerPathQ = QString::fromUtf8(ownerPath.data(), static_cast<int>(ownerPath.length()));

        auto group = makeShared<PropertyGroup>();
        group->setName(QString::fromUtf8(uri.data(), static_cast<int>(uri.length())));

        auto path = PropertyPath(ownerPathQ);

        for (int c = 0; c < table.columnCount(); ++c) {
            auto colName = table.columnName(c);
            auto prop = makeShared<AbstractProperty>();
            const QString colNameQ = QString::fromUtf8(colName.data(), static_cast<int>(colName.length()));
            prop->setName(colNameQ);
            prop->setType(PropertyType::String);
            prop->setDisplayLabel(colNameQ);

            ZeroString preview;
            for (int r = 0; r < std::min(table.rowCount(), 3); ++r) {
                if (r > 0) preview += ", ";
                preview += toStdString(table.getString(r, c));
            }
            if (table.rowCount() > 3) preview += "...";
            prop->setValue(QString::fromUtf8(preview.data(), static_cast<int>(preview.length())));

            group->addProperty(prop);
        }

        globalPropertyRegistry().registerOwnerSnapshot(
            ownerPathQ,
            QString::fromUtf8(uri.data(), static_cast<int>(uri.length())),
            QString("Data"),
            *group);
        registeredSources_[toStdString(uri)] = source;
    }

    static void unregisterSource(const String& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        ZeroString ownerPath = ZeroString("data.") + sanitizePath(uri);
        globalPropertyRegistry().unregisterOwner(QString::fromUtf8(ownerPath.data(), static_cast<int>(ownerPath.length())));
        registeredSources_.erase(toStdString(uri));
        DataCache::instance().invalidate(uri);
        FileWatcher::instance().unwatch(uri);
    }

    static std::vector<String> registeredUris() {
        std::lock_guard<std::mutex> lock(mutex_);
        NamedVector<String> uris;
        uris.reserve(registeredSources_.size());
        for (const auto& [uri, _] : registeredSources_) {
            uris.emplace_back(uri);
        }
        return uris.toStdVector();
    }

    static String getValue(const String& uri, int row, const String& column) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredSources_.find(toStdString(uri));
        if (it == registeredSources_.end()) return "";
        return String(it->second->table().getString(row, toStdString(column)));
    }

    static int getRowCount(const String& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredSources_.find(toStdString(uri));
        if (it == registeredSources_.end()) return 0;
        return it->second->table().rowCount();
    }

    static int getColumnCount(const String& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredSources_.find(toStdString(uri));
        if (it == registeredSources_.end()) return 0;
        return it->second->table().columnCount();
    }

private:
    static ZeroString sanitizePath(std::string_view s) {
        ZeroString result;
        result.reserve(s.size());
        for (char c : s) {
            if (c == '/' || c == '\\') result += '.';
            else if (c == ' ') result += '_';
            else if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.') result += c;
        }
        return result;
    }

    static ZeroString sanitizePath(StringView s) {
        return sanitizePath(std::string_view(s.data(), s.length()));
    }

    static ZeroString sanitizePath(const String& s) {
        return sanitizePath(std::string_view(s.data(), s.length()));
    }

    static std::unordered_map<std::string, DataSourcePtr> registeredSources_;
    static std::mutex mutex_;
};

inline std::unordered_map<std::string, DataSourcePtr> DataPropertyBridge::registeredSources_;
inline std::mutex DataPropertyBridge::mutex_;

} // namespace ArtifactCore
