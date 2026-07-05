module;
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QHash>

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
  bool valueBool(const QString& key, bool defaultValue = false) const;
  qlonglong valueInt64(const QString& key, qlonglong defaultValue = 0) const;
  QString valueString(const QString& key, const QString& defaultValue = QString()) const;
  void setValue(const QString& key, const QVariant& value);
  void remove(const QString& key);
  void clear();

  QStringList keys() const;
  int size() const;

  void beginBatch();
  void endBatch(bool forceSync = true);
  void setAutoSyncThreshold(int operations);
  int autoSyncThreshold() const;
  bool isDirty() const;
  int pendingOperations() const;
  bool sync();
 };
}
