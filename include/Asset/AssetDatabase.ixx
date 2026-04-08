module;
#include <utility>
#include <memory>
#include <QString>
#include <QUuid>
#include <QMap>
#include <QList>

export module Asset.Database;

import AssetType;

export namespace ArtifactCore {

/**
 * @brief Represents basic information about an asset in the project
 */
struct AssetInfo {
    QUuid id;
    QString name;
    QString absolutePath;
    AssetType type;
    QMap<QString, QString> metadata;
};

/**
 * @brief Central database for managing all assets in the project
 * 
 * It tracks file paths, UUIDs, and types to allow fast searching 
 * and persistent references.
 */
class AssetDatabase {
public:
    static AssetDatabase& instance() {
        static AssetDatabase db;
        return db;
    }

    // Asset Management
    QUuid registerAsset(const QString& path, AssetType type);
    void unregisterAsset(const QUuid& id);
    
    // Recovery
    AssetInfo getAssetInfo(const QUuid& id) const;
    QUuid findAssetByPath(const QString& path) const;
    QList<AssetInfo> findAssetsByType(AssetType type) const;
    QList<AssetInfo> allAssets() const;

    // Persistence
    bool load(const QString& databasePath);
    bool save(const QString& databasePath) const;

private:
    AssetDatabase() = default;
    QMap<QUuid, AssetInfo> assets_;
    QMap<QString, QUuid> pathToId_;
};

} // namespace ArtifactCore
