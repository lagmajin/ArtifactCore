module;
#include <utility>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QSettings>
export module UI.Layout.State;

import Core.FastSettingsStore;

export namespace ArtifactCore
{
 class UiLayoutState
 {
 public:
  QString layoutKey;
  QByteArray geometry;
  QByteArray state;
  // ADS (Advanced Docking System) のレイアウト状態。
  // CDockManager::saveState() の戻り値を格納し、restoreState() で復元する。
  // QMainWindow::saveState() には ADS の dock 配置が含まれないため、別途保持する。
  QByteArray dockState;
  int version = 2;

  UiLayoutState() = default;
  explicit UiLayoutState(QString key) : layoutKey(std::move(key)) {}

  bool isEmpty() const;
  QJsonObject toJson() const;
  static UiLayoutState fromJson(const QJsonObject& obj);

  void saveToSettings(QSettings& settings, const QString& prefix) const;
  static UiLayoutState loadFromSettings(QSettings& settings, const QString& prefix);
  void saveToStore(FastSettingsStore& store, const QString& prefix) const;
  static UiLayoutState loadFromStore(FastSettingsStore& store, const QString& prefix);
 };
}
