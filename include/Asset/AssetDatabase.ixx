module;
class tst_QList;
#include <utility>
#include <memory>
#include <QString>
#include <QByteArray>
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
    QUuid registerAsset(const QString& path, AssetType type, const QUuid& preferredId);
    void unregisterAsset(const QUuid& id);
    // Drop every registration. The database is a per-process identity cache,
    // and the project document is the authority for what an asset's UUID is,
    // so a project switch must not carry the previous project's paths over.
    void clear();
    
    // Recovery
    AssetInfo getAssetInfo(const QUuid& id) const;
    QUuid findAssetByPath(const QString& path) const;
    // Move an existing asset identity to a new normalized path. The asset ID
    // is preserved; collisions and missing old paths leave the database unchanged.
    bool relinkAssetPath(const QString& oldPath, const QString& newPath);
    QList<AssetInfo> findAssetsByType(AssetType type) const;
    QList<AssetInfo> allAssets() const;

    // Persistence
    // The database holds no file of its own on purpose. Asset identity is
    // carried by the project document (per-footage `assetId`) and by the
    // per-asset `<path>.assetmeta` sidecar, and both are replayed on load
    // through registerAsset(). These two entry points stay for exporting a
    // snapshot of the current registrations, not as a save/load pair.
    QByteArray toJson() const;
    bool fromJson(const QByteArray& payload);

private:
    AssetDatabase() = default;
    QMap<QUuid, AssetInfo> assets_;
    QMap<QString, QUuid> pathToId_;
};

} // namespace ArtifactCore
