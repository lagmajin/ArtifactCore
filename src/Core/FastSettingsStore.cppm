module;
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QVariant>
#include <QVariantMap>
#include <QCborMap>
#include <QCborValue>
#include <QByteArray>
#include <QStringList>

#include <algorithm>
#include <mutex>
#include <shared_mutex>

module Core.FastSettingsStore;

namespace ArtifactCore
{
 class FastSettingsStore::Impl
 {
 public:
  QString path_;
  QHash<QString, QVariant> cache_;
  mutable std::shared_mutex mutex_;
  bool dirty_ = false;
  bool opened_ = false;
  int autoSyncThreshold_ = 128;
  int pendingOps_ = 0;
  int batchDepth_ = 0;

  static QVariantMap hashToVariantMap(const QHash<QString, QVariant>& hash)
  {
    QVariantMap map;
    for (auto it = hash.constBegin(); it != hash.constEnd(); ++it) {
      map.insert(it.key(), it.value());
    }
    return map;
  }

  static QHash<QString, QVariant> variantMapToHash(const QVariantMap& map)
  {
    QHash<QString, QVariant> out;
    out.reserve(map.size());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
      out.insert(it.key(), it.value());
    }
    return out;
  }

  bool ensureParentDirExists() const
  {
    if (path_.isEmpty()) {
      return false;
    }
    QFileInfo fi(path_);
    const QDir dir = fi.dir();
    if (dir.exists()) {
      return true;
    }
    QDir mk;
    return mk.mkpath(dir.absolutePath());
  }

  bool loadFromDiskUnlocked()
  {
    cache_.clear();
    dirty_ = false;
    pendingOps_ = 0;

    if (path_.isEmpty()) {
      return false;
    }
    QFile file(path_);
    if (!file.exists()) {
      return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
      return false;
    }
    const QByteArray payload = file.readAll();
    file.close();
    if (payload.isEmpty()) {
      return true;
    }

    const QCborValue cbor = QCborValue::fromCbor(payload);
    if (!cbor.isMap()) {
      return false;
    }
    const QVariantMap map = cbor.toMap().toVariantMap();
    cache_ = variantMapToHash(map);
    return true;
  }

  bool syncUnlocked()
  {
    if (!opened_ || path_.isEmpty()) {
      return false;
    }
    if (!dirty_) {
      return true;
    }
    if (!ensureParentDirExists()) {
      return false;
    }

    const QVariantMap map = hashToVariantMap(cache_);
    const QCborMap cborMap = QCborMap::fromVariantMap(map);
    const QByteArray payload = QCborValue(cborMap).toCbor();

    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
      return false;
    }
    if (file.write(payload) != payload.size()) {
      file.cancelWriting();
      return false;
    }
    if (!file.commit()) {
      return false;
    }

    dirty_ = false;
    pendingOps_ = 0;
    return true;
  }

  void markChangedUnlocked()
  {
    dirty_ = true;
    ++pendingOps_;
  }
 };

 FastSettingsStore::FastSettingsStore() : impl_(new Impl())
 {
 }

 FastSettingsStore::FastSettingsStore(const QString& filePath) : impl_(new Impl())
 {
  open(filePath);
 }

 FastSettingsStore::~FastSettingsStore()
 {
  if (impl_) {
    std::unique_lock lock(impl_->mutex_);
    if (impl_->dirty_) {
      impl_->syncUnlocked();
    }
  }
  delete impl_;
 }

 bool FastSettingsStore::open(const QString& filePath)
 {
  if (!impl_) {
    return false;
  }
  std::unique_lock lock(impl_->mutex_);
  impl_->path_ = filePath.trimmed();
  impl_->opened_ = !impl_->path_.isEmpty();
  if (!impl_->opened_) {
    impl_->cache_.clear();
    impl_->dirty_ = false;
    impl_->pendingOps_ = 0;
    return false;
  }
  return impl_->loadFromDiskUnlocked();
 }

 QString FastSettingsStore::filePath() const
 {
  if (!impl_) {
    return {};
  }
  std::shared_lock lock(impl_->mutex_);
  return impl_->path_;
 }

 bool FastSettingsStore::contains(const QString& key) const
 {
  if (!impl_) {
    return false;
  }
  std::shared_lock lock(impl_->mutex_);
  return impl_->cache_.contains(key);
 }

 QVariant FastSettingsStore::value(const QString& key, const QVariant& defaultValue) const
 {
  if (!impl_) {
    return defaultValue;
  }
  std::shared_lock lock(impl_->mutex_);
  return impl_->cache_.value(key, defaultValue);
 }

 void FastSettingsStore::setValue(const QString& key, const QVariant& value)
 {
  if (!impl_ || key.isEmpty()) {
    return;
  }
  std::unique_lock lock(impl_->mutex_);
  impl_->cache_.insert(key, value);
  impl_->markChangedUnlocked();
  if (impl_->batchDepth_ == 0 && impl_->pendingOps_ >= impl_->autoSyncThreshold_) {
    impl_->syncUnlocked();
  }
 }

 void FastSettingsStore::remove(const QString& key)
 {
  if (!impl_ || key.isEmpty()) {
    return;
  }
  std::unique_lock lock(impl_->mutex_);
  if (impl_->cache_.remove(key) > 0) {
    impl_->markChangedUnlocked();
    if (impl_->batchDepth_ == 0 && impl_->pendingOps_ >= impl_->autoSyncThreshold_) {
      impl_->syncUnlocked();
    }
  }
 }

 void FastSettingsStore::clear()
 {
  if (!impl_) {
    return;
  }
  std::unique_lock lock(impl_->mutex_);
  if (impl_->cache_.isEmpty()) {
    return;
  }
  impl_->cache_.clear();
  impl_->markChangedUnlocked();
  if (impl_->batchDepth_ == 0) {
    impl_->syncUnlocked();
  }
 }

 QStringList FastSettingsStore::keys() const
 {
  if (!impl_) {
    return {};
  }
  std::shared_lock lock(impl_->mutex_);
  QStringList out;
  out.reserve(impl_->cache_.size());
  for (auto it = impl_->cache_.constBegin(); it != impl_->cache_.constEnd(); ++it) {
    out.push_back(it.key());
  }
  return out;
 }

 int FastSettingsStore::size() const
 {
  if (!impl_) {
    return 0;
  }
  std::shared_lock lock(impl_->mutex_);
  return impl_->cache_.size();
 }

 void FastSettingsStore::beginBatch()
 {
  if (!impl_) {
    return;
  }
  std::unique_lock lock(impl_->mutex_);
  ++impl_->batchDepth_;
 }

 void FastSettingsStore::endBatch(bool forceSync)
 {
  if (!impl_) {
    return;
  }
  std::unique_lock lock(impl_->mutex_);
  if (impl_->batchDepth_ > 0) {
    --impl_->batchDepth_;
  }
  if (impl_->batchDepth_ == 0 && impl_->dirty_ && forceSync) {
    impl_->syncUnlocked();
  }
 }

 void FastSettingsStore::setAutoSyncThreshold(int operations)
 {
  if (!impl_) {
    return;
  }
  std::unique_lock lock(impl_->mutex_);
  impl_->autoSyncThreshold_ = std::max(1, operations);
 }

 bool FastSettingsStore::sync()
 {
  if (!impl_) {
    return false;
  }
  std::unique_lock lock(impl_->mutex_);
  return impl_->syncUnlocked();
 }
}
