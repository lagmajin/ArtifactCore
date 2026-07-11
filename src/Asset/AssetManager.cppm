module;
#include <QFileSystemWatcher>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <vector>

module Asset.Manager;

import Asset.Database;

namespace ArtifactCore {

 class AssetManager::Impl {
 public:
  struct SourceState {
   int useCount = 0;
   std::uint64_t version = 1;
   QUuid originAssetId;
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

 void AssetManager::resetSourceRegistry()
 {
  QMutexLocker locker(&impl_->mutex);
  impl_->sources.clear();
  impl_->decodedPayloads.clear();
 }

 QUuid AssetManager::acquireSource(const QString& path, AssetType type)
 {
  const QUuid assetId = AssetDatabase::instance().registerAsset(path, type);
  if (assetId.isNull()) {
   return {};
  }
  QMutexLocker locker(&impl_->mutex);
  auto& state = impl_->sources[assetId];
  if (state.originAssetId.isNull()) {
   state.originAssetId = assetId;
  }
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

 QUuid AssetManager::localizeSource(const QUuid& assetId)
 {
  if (assetId.isNull()) {
   return {};
  }
  QMutexLocker locker(&impl_->mutex);
  auto sourceIt = impl_->sources.find(assetId);
  if (sourceIt == impl_->sources.end() || sourceIt->useCount <= 0) {
   return {};
  }

  const QUuid localizedId = QUuid::createUuid();
  Impl::SourceState localized = sourceIt.value();
  localized.useCount = 1;
  if (localized.originAssetId.isNull()) {
   localized.originAssetId = assetId;
  }
  --sourceIt->useCount;
  impl_->sources.insert(localizedId, localized);

  const QString oldPrefix = assetId.toString(QUuid::WithoutBraces) + QStringLiteral(":");
  const QString newPrefix = localizedId.toString(QUuid::WithoutBraces) + QStringLiteral(":");
  QHash<QString, std::weak_ptr<void>> payloadCopies;
  for (auto payloadIt = impl_->decodedPayloads.cbegin();
       payloadIt != impl_->decodedPayloads.cend(); ++payloadIt) {
   if (payloadIt.key().startsWith(oldPrefix)) {
    payloadCopies.insert(newPrefix + payloadIt.key().mid(oldPrefix.size()),
                         payloadIt.value());
   }
  }
  for (auto it = payloadCopies.cbegin(); it != payloadCopies.cend(); ++it) {
   impl_->decodedPayloads.insert(it.key(), it.value());
  }
  return localizedId;
 }

 bool AssetManager::acquireExistingSource(const QUuid& assetId)
 {
  QMutexLocker locker(&impl_->mutex);
  auto it = impl_->sources.find(assetId);
  if (it == impl_->sources.end()) {
   return false;
  }
  ++it->useCount;
  return true;
 }

 bool AssetManager::isLocalizedSource(const QUuid& assetId) const
 {
  QMutexLocker locker(&impl_->mutex);
  const auto it = impl_->sources.constFind(assetId);
  return it != impl_->sources.cend() && !it->originAssetId.isNull() &&
         it->originAssetId != assetId;
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
   const QUuid originId = it->originAssetId.isNull() ? it.key() : it->originAssetId;
   const auto info = AssetDatabase::instance().getAssetInfo(originId);
   if (info.id.isNull() || info.absolutePath.isEmpty()) {
    continue;
   }
   QJsonObject source;
   source.insert(QStringLiteral("id"), it.key().toString(QUuid::WithoutBraces));
   source.insert(QStringLiteral("originId"), originId.toString(QUuid::WithoutBraces));
   source.insert(QStringLiteral("localized"), originId != it.key());
   source.insert(QStringLiteral("path"), info.absolutePath);
   source.insert(QStringLiteral("type"), static_cast<int>(info.type));
   source.insert(QStringLiteral("version"), QString::number(it->version));
   sources.append(source);
  }
  QJsonObject snapshot;
  snapshot.insert(QStringLiteral("schemaVersion"), 2);
  snapshot.insert(QStringLiteral("sources"), sources);
  return snapshot;
 }

 bool AssetManager::restoreSourceRegistrySnapshot(const QJsonObject& snapshot)
 {
  if (snapshot.isEmpty()) {
   return true;
  }
  const QJsonValue schemaValue = snapshot.value(QStringLiteral("schemaVersion"));
  if (!schemaValue.isUndefined() &&
      (!schemaValue.isDouble() || schemaValue.toInt(-1) > 2 ||
       schemaValue.toInt(-1) < 1)) {
   return false;
  }
  const QJsonValue sourcesValue = snapshot.value(QStringLiteral("sources"));
  if (!sourcesValue.isArray()) {
   return false;
  }

  struct PendingSource {
   QUuid preferredId;
   QUuid originId;
   QString path;
   AssetType type = AssetType::Unknown;
   std::uint64_t version = 0;
   bool localized = false;
  };
  std::vector<PendingSource> pending;
  pending.reserve(sourcesValue.toArray().size());
  for (const QJsonValue& value : sourcesValue.toArray()) {
   if (!value.isObject()) {
    return false;
   }
   const QJsonObject source = value.toObject();
   const QUuid preferredId(source.value(QStringLiteral("id")).toString());
   const QUuid originId(source.value(QStringLiteral("originId")).toString());
   const bool localized = source.value(QStringLiteral("localized")).toBool(false);
   const QString path = source.value(QStringLiteral("path")).toString();
   const auto type = static_cast<AssetType>(
       source.value(QStringLiteral("type")).toInt(static_cast<int>(AssetType::Unknown)));
   bool versionOk = false;
   const std::uint64_t version = source.value(QStringLiteral("version"))
                                     .toString().toULongLong(&versionOk);
   if (preferredId.isNull() || (localized && originId.isNull()) ||
       path.trimmed().isEmpty() || !versionOk || version == 0) {
    return false;
   }
   pending.push_back({preferredId, originId, path, type, version, localized});
  }

  std::vector<PendingSource> resolved;
  resolved.reserve(pending.size());
  for (const PendingSource& entry : pending) {
   const QUuid databaseId = entry.localized && !entry.originId.isNull()
                                ? entry.originId
                                : entry.preferredId;
   const QUuid originAssetId = AssetDatabase::instance().registerAsset(
       entry.path, entry.type, databaseId);
   const QUuid assetId = entry.localized ? entry.preferredId : originAssetId;
   if (assetId.isNull()) {
    return false;
   }
   PendingSource resolvedEntry = entry;
   resolvedEntry.originId = originAssetId;
   resolved.push_back(resolvedEntry);
  }

  {
   QMutexLocker locker(&impl_->mutex);
   for (const PendingSource& entry : resolved) {
    const QUuid assetId = entry.localized ? entry.preferredId : entry.originId;
    auto& state = impl_->sources[assetId];
    state.version = std::max(state.version, entry.version);
    state.originAssetId = entry.originId;
   }
  }
  return true;
 }

 QJsonArray AssetManager::sourceHealthSnapshot() const
 {
  QJsonArray health;
  QMutexLocker locker(&impl_->mutex);
  for (auto it = impl_->sources.cbegin(); it != impl_->sources.cend(); ++it) {
   const QUuid originId = it->originAssetId.isNull() ? it.key() : it->originAssetId;
   const auto info = AssetDatabase::instance().getAssetInfo(originId);
   if (info.id.isNull() || info.absolutePath.isEmpty()) {
    continue;
   }
   const QString prefix = it.key().toString(QUuid::WithoutBraces) + QStringLiteral(":");
   int livePayloadCount = 0;
   for (auto payloadIt = impl_->decodedPayloads.cbegin();
        payloadIt != impl_->decodedPayloads.cend(); ++payloadIt) {
    if (payloadIt.key().startsWith(prefix) && !payloadIt.value().expired()) {
     ++livePayloadCount;
    }
   }
   QJsonObject source;
   source.insert(QStringLiteral("id"), it.key().toString(QUuid::WithoutBraces));
   source.insert(QStringLiteral("path"), info.absolutePath);
   source.insert(QStringLiteral("type"), static_cast<int>(info.type));
   source.insert(QStringLiteral("version"), QString::number(it->version));
   source.insert(QStringLiteral("useCount"), it->useCount);
   source.insert(QStringLiteral("livePayloadCount"), livePayloadCount);
   source.insert(QStringLiteral("localized"), originId != it.key());
   health.append(source);
  }
  return health;
 }

};
