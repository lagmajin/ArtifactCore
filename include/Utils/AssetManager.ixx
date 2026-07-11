module;
#include <utility>
#include <QString>
#include <QUuid>
#include <cstdint>
#include <memory>
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
   std::shared_ptr<void> decodedPayload(
       const QUuid& assetId, std::uint64_t version,
       const QString& representation) const;
   std::shared_ptr<void> publishDecodedPayload(
       const QUuid& assetId, std::uint64_t version,
       const QString& representation, std::shared_ptr<void> payload);
 };

};
