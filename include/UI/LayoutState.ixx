module;
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QSettings>

export module UI.Layout.State;

export namespace ArtifactCore
{
 class UiLayoutState
 {
 public:
  QString layoutKey;
  QByteArray geometry;
  QByteArray state;
  int version = 1;

  UiLayoutState() = default;
  explicit UiLayoutState(QString key) : layoutKey(std::move(key)) {}

  bool isEmpty() const;
  QJsonObject toJson() const;
  static UiLayoutState fromJson(const QJsonObject& obj);

  void saveToSettings(QSettings& settings, const QString& prefix) const;
  static UiLayoutState loadFromSettings(QSettings& settings, const QString& prefix);
 };
}
