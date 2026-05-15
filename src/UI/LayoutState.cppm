module;
#include <utility>

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QSettings>

module UI.Layout.State;

import Core.FastSettingsStore;

namespace ArtifactCore
{
 bool UiLayoutState::isEmpty() const
 {
  return geometry.isEmpty() && state.isEmpty();
 }

 QJsonObject UiLayoutState::toJson() const
 {
  QJsonObject obj;
  obj["layoutKey"] = layoutKey;
  obj["version"] = version;
  obj["geometry_b64"] = QString::fromLatin1(geometry.toBase64());
  obj["state_b64"] = QString::fromLatin1(state.toBase64());
  return obj;
 }

 UiLayoutState UiLayoutState::fromJson(const QJsonObject& obj)
 {
  UiLayoutState s;
  s.layoutKey = obj.value("layoutKey").toString();
  s.version = obj.value("version").toInt(1);
  s.geometry = QByteArray::fromBase64(obj.value("geometry_b64").toString().toLatin1());
  s.state = QByteArray::fromBase64(obj.value("state_b64").toString().toLatin1());
  return s;
 }

 void UiLayoutState::saveToSettings(QSettings& settings, const QString& prefix) const
 {
  settings.setValue(prefix + "/layoutKey", layoutKey);
  settings.setValue(prefix + "/version", version);
  settings.setValue(prefix + "/geometry", geometry);
  settings.setValue(prefix + "/state", state);
 }

 UiLayoutState UiLayoutState::loadFromSettings(QSettings& settings, const QString& prefix)
 {
  UiLayoutState s;
  s.layoutKey = settings.value(prefix + "/layoutKey", QString()).toString();
  s.version = settings.value(prefix + "/version", 1).toInt();
  s.geometry = settings.value(prefix + "/geometry", QByteArray()).toByteArray();
  s.state = settings.value(prefix + "/state", QByteArray()).toByteArray();
  return s;
 }

 void UiLayoutState::saveToStore(FastSettingsStore& store, const QString& prefix) const
 {
  store.setValue(prefix + "/layoutKey", layoutKey);
  store.setValue(prefix + "/version", version);
  store.setValue(prefix + "/geometry", geometry);
  store.setValue(prefix + "/state", state);
 }

 UiLayoutState UiLayoutState::loadFromStore(FastSettingsStore& store, const QString& prefix)
 {
  UiLayoutState s;
  s.layoutKey = store.value(prefix + "/layoutKey", QString()).toString();
  s.version = store.value(prefix + "/version", 1).toInt();
  s.geometry = store.value(prefix + "/geometry", QByteArray()).toByteArray();
  s.state = store.value(prefix + "/state", QByteArray()).toByteArray();
  return s;
 }
}
