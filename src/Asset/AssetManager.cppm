module;
#include <QFileSystemWatcher>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

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

 QJsonObject AssetManager::sourceRegistrySnapshot() const
 {
  QJsonArray sources;
  QMutexLocker locker(&impl_->mutex);
  for (auto it = impl_->sources.cbegin(); it != impl_->sources.cend(); ++it) {
   const auto info = AssetDatabase::instance().getAssetInfo(it.key());
   if (info.id.isNull() || info.absolutePath.isEmpty()) {
    continue;
   }
   QJsonObject source;
   source.insert(QStringLiteral("id"), info.id.toString(QUuid::WithoutBraces));
   source.insert(QStringLiteral("path"), info.absolutePath);
   source.insert(QStringLiteral("type"), static_cast<int>(info.type));
   source.insert(QStringLiteral("version"), QString::number(it->version));
   sources.append(source);
  }
  QJsonObject snapshot;
  snapshot.insert(QStringLiteral("schemaVersion"), 1);
  snapshot.insert(QStringLiteral("sources"), sources);
  return snapshot;
 }

 bool AssetManager::restoreSourceRegistrySnapshot(const QJsonObject& snapshot)
 {
  if (snapshot.isEmpty()) {
   return true;
  }
  const QJsonValue sourcesValue = snapshot.value(QStringLiteral("sources"));
  if (!sourcesValue.isArray()) {
   return false;
  }

  bool valid = true;
  for (const QJsonValue& value : sourcesValue.toArray()) {
   if (!value.isObject()) {
    valid = false;
    continue;
   }
   const QJsonObject source = value.toObject();
   const QUuid preferredId(source.value(QStringLiteral("id")).toString());
   const QString path = source.value(QStringLiteral("path")).toString();
   const auto type = static_cast<AssetType>(
       source.value(QStringLiteral("type")).toInt(static_cast<int>(AssetType::Unknown)));
   bool versionOk = false;
   const std::uint64_t version = source.value(QStringLiteral("version"))
                                     .toString().toULongLong(&versionOk);
   if (preferredId.isNull() || path.trimmed().isEmpty() || !versionOk || version == 0) {
    valid = false;
    continue;
   }
   const QUuid assetId = AssetDatabase::instance().registerAsset(
       path, type, preferredId);
   if (assetId.isNull()) {
    valid = false;
    continue;
   }
   QMutexLocker locker(&impl_->mutex);
   auto& state = impl_->sources[assetId];
   state.version = std::max(state.version, version);
  }
  return valid;
 }

};
