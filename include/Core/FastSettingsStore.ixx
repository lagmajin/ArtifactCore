module;
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QHash>

#include <shared_mutex>

export module Core.FastSettingsStore;

export namespace ArtifactCore
{
 class FastSettingsStore
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  FastSettingsStore();
  explicit FastSettingsStore(const QString& filePath);
  ~FastSettingsStore();

  FastSettingsStore(const FastSettingsStore&) = delete;
  FastSettingsStore& operator=(const FastSettingsStore&) = delete;

  bool open(const QString& filePath);
  QString filePath() const;

  bool contains(const QString& key) const;
  QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
  void setValue(const QString& key, const QVariant& value);
  void remove(const QString& key);
  void clear();

  QStringList keys() const;
  int size() const;

  void beginBatch();
  void endBatch(bool forceSync = true);
  void setAutoSyncThreshold(int operations);
  bool sync();
 };
}
