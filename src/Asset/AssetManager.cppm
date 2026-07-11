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
  QHash<QString, std::weak_ptr<void>> decodedPayloads;
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
  const std::uint64_t nextVersion = ++state.version;
  const QString prefix = assetId.toString(QUuid::WithoutBraces) + QStringLiteral(":");
  for (auto it = impl_->decodedPayloads.begin(); it != impl_->decodedPayloads.end();) {
   if (it.key().startsWith(prefix)) {
    it = impl_->decodedPayloads.erase(it);
   } else {
    ++it;
   }
  }
  return nextVersion;
 }

 namespace {
 QString decodedPayloadKey(const QUuid& assetId, std::uint64_t version,
                           const QString& representation)
 {
  return assetId.toString(QUuid::WithoutBraces) + QStringLiteral(":") +
         QString::number(version) + QStringLiteral(":") +
         representation.trimmed();
 }
 }

 std::shared_ptr<void> AssetManager::decodedPayload(
     const QUuid& assetId, std::uint64_t version,
     const QString& representation) const
 {
  if (assetId.isNull() || version == 0 || representation.trimmed().isEmpty()) {
   return {};
  }
  QMutexLocker locker(&impl_->mutex);
  const auto it = impl_->decodedPayloads.constFind(
      decodedPayloadKey(assetId, version, representation));
  return it == impl_->decodedPayloads.cend() ? std::shared_ptr<void>{}
                                             : it->lock();
 }

 std::shared_ptr<void> AssetManager::publishDecodedPayload(
     const QUuid& assetId, std::uint64_t version,
     const QString& representation, std::shared_ptr<void> payload)
 {
  const QString normalizedRepresentation = representation.trimmed();
  if (assetId.isNull() || version == 0 || normalizedRepresentation.isEmpty() ||
      !payload) {
   return {};
  }
  QMutexLocker locker(&impl_->mutex);
  const auto stateIt = impl_->sources.constFind(assetId);
  if (stateIt == impl_->sources.cend() || stateIt->version != version) {
   return {};
  }
  const QString key = decodedPayloadKey(assetId, version, normalizedRepresentation);
  const auto existing = impl_->decodedPayloads.value(key).lock();
  if (existing) {
   return existing;
  }
  impl_->decodedPayloads.insert(key, payload);
  return payload;
 }

};
