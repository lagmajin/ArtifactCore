module;

#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

export module Serialization.SchemaMigration;

export namespace ArtifactCore::Serialization {

class SchemaMigrationRegistry {
public:
    using Migration = std::function<QJsonObject(const QJsonObject&)>;

    static SchemaMigrationRegistry& instance()
    {
        static SchemaMigrationRegistry registry;
        return registry;
    }

    void registerMigration(const QString& typeName, int fromVersion,
                           int toVersion, Migration migration)
    {
        const QString normalizedTypeName = typeName.trimmed();
        if (normalizedTypeName.isEmpty() || fromVersion < 0 || toVersion <= fromVersion || !migration) {
            return;
        }
        std::lock_guard lock(mutex_);
        migrations_[normalizedTypeName.toStdString()][fromVersion][toVersion] = std::move(migration);
    }

    bool hasMigrationPath(const QString& typeName, int fromVersion, int toVersion) const
    {
        QJsonObject ignored;
        return findPath(typeName.trimmed(), fromVersion, toVersion, ignored, false);
    }

    std::vector<int> availableVersions(const QString& typeName) const
    {
        std::vector<int> versions;
        std::lock_guard lock(mutex_);
        const auto typeIt = migrations_.find(typeName.trimmed().toStdString());
        if (typeIt == migrations_.end()) {
            return versions;
        }
        for (const auto& [fromVersion, targets] : typeIt->second) {
            versions.push_back(fromVersion);
            for (const auto& [toVersion, migration] : targets) {
                (void)migration;
                versions.push_back(toVersion);
            }
        }
        std::sort(versions.begin(), versions.end());
        versions.erase(std::unique(versions.begin(), versions.end()), versions.end());
        return versions;
    }

    bool migrate(const QString& typeName, int fromVersion, int toVersion,
                 QJsonObject& data) const
    {
        return findPath(typeName.trimmed(), fromVersion, toVersion, data, true);
    }

private:
    using VersionMap = std::map<int, std::map<int, Migration>>;
    std::map<std::string, VersionMap> migrations_;
    mutable std::mutex mutex_;

    bool findPath(const QString& typeName, int fromVersion, int toVersion,
                  QJsonObject& data, bool apply) const
    {
        if (fromVersion < 0 || toVersion < 0) {
            return false;
        }
        if (fromVersion == toVersion) {
            return true;
        }
        if (fromVersion > toVersion) {
            return false;
        }

        VersionMap typeMigrations;
        {
            std::lock_guard lock(mutex_);
            const auto typeIt = migrations_.find(typeName.toStdString());
            if (typeIt == migrations_.end()) {
                return false;
            }
            typeMigrations = typeIt->second;
        }
        if (typeMigrations.empty()) {
            return false;
        }

        std::vector<int> pending{fromVersion};
        std::set<int> visited{fromVersion};
        std::map<int, std::pair<int, Migration>> parents;
        for (std::size_t index = 0; index < pending.size(); ++index) {
            const int version = pending[index];
            const auto fromIt = typeMigrations.find(version);
            if (fromIt == typeMigrations.end()) {
                continue;
            }
            for (const auto& [nextVersion, migration] : fromIt->second) {
                if (nextVersion > toVersion || visited.contains(nextVersion)) {
                    continue;
                }
                visited.insert(nextVersion);
                parents.emplace(nextVersion,
                                std::make_pair(version, migration));
            pending.push_back(nextVersion);
            }
        }
        if (!visited.contains(toVersion)) {
            return false;
        }

        std::vector<Migration> path;
        for (int version = toVersion; version != fromVersion;) {
            const auto parent = parents.find(version);
            if (parent == parents.end()) {
                return false;
            }
            path.push_back(parent->second.second);
            version = parent->second.first;
        }
        if (apply) {
            for (auto migration = path.rbegin(); migration != path.rend(); ++migration) {
                data = (*migration)(data);
            }
        }
        return true;
    }
};

} // namespace ArtifactCore::Serialization
