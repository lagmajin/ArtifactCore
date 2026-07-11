module;
#include <utility>
#include <QString>
#include <QUuid>
#include <cstdint>
export module Asset.Manager;

import AssetType;

export namespace ArtifactCore {

 class AssetManager {
  private:
   class Impl;
   Impl* impl_;
  public:
   static AssetManager& instance();
   AssetManager();
   ~AssetManager();
   AssetManager(const AssetManager&) = delete;
   AssetManager& operator=(const AssetManager&) = delete;

   QUuid acquireSource(const QString& path, AssetType type);
   bool releaseSource(const QUuid& assetId);
   QUuid sourceId(const QString& path) const;
   int useCount(const QUuid& assetId) const;
   std::uint64_t sourceVersion(const QUuid& assetId) const;
   std::uint64_t invalidateSource(const QUuid& assetId);
 };

};
