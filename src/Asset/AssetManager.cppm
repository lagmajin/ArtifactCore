module;
#include <QFileSystemWatcher>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

module Asset.Manager;

import Asset.Database;

namespace ArtifactCore {

 class AssetManager::Impl {
 public:
  struct SourceState {
   int useCount = 0;
   std::uint64_t version = 1;
  };

  mutable QMutex mutex;
  QHash<QUuid, SourceState> sources;
  Impl() = default;
  ~Impl() = default;
 };

 AssetManager& AssetManager::instance()
 {
  static AssetManager manager;
  return manager;
 }

 AssetManager::AssetManager() : impl_(new Impl())
 {
 }

 AssetManager::~AssetManager()
 {
  delete impl_;
 }

 QUuid AssetManager::acquireSource(const QString& path, AssetType type)
 {
  const QUuid assetId = AssetDatabase::instance().registerAsset(path, type);
  if (assetId.isNull()) {
   return {};
  }
  QMutexLocker locker(&impl_->mutex);
  auto& state = impl_->sources[assetId];
  ++state.useCount;
  return assetId;
 }

 bool AssetManager::releaseSource(const QUuid& assetId)
 {
  if (assetId.isNull()) {
   return false;
  }
  QMutexLocker locker(&impl_->mutex);
  auto it = impl_->sources.find(assetId);
  if (it == impl_->sources.end() || it->useCount <= 0) {
   return false;
  }
  --it->useCount;
  return true;
 }

 QUuid AssetManager::sourceId(const QString& path) const
 {
  return AssetDatabase::instance().findAssetByPath(path);
 }

 int AssetManager::useCount(const QUuid& assetId) const
 {
  QMutexLocker locker(&impl_->mutex);
  const auto it = impl_->sources.constFind(assetId);
  return it == impl_->sources.cend() ? 0 : it->useCount;
 }

 std::uint64_t AssetManager::sourceVersion(const QUuid& assetId) const
 {
  QMutexLocker locker(&impl_->mutex);
  const auto it = impl_->sources.constFind(assetId);
  return it == impl_->sources.cend() ? 0 : it->version;
 }

 std::uint64_t AssetManager::invalidateSource(const QUuid& assetId)
 {
  if (assetId.isNull()) {
   return 0;
  }
  QMutexLocker locker(&impl_->mutex);
  auto& state = impl_->sources[assetId];
  return ++state.version;
 }

};
