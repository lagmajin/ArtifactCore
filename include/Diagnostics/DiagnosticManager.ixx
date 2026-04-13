module;
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <mutex>
#include <vector>


export module Diagnostics.DiagnosticManager;

import Diagnostics.DiagnosticMarker;

export namespace ArtifactCore {

class DiagnosticManager {
private:
  std::vector<DiagnosticMarker> markers_;
  mutable std::mutex mutex_;

public:
  void addMarker(const DiagnosticMarker &marker) {
    std::lock_guard<std::mutex> lock(mutex_);
    markers_.push_back(marker);
  }

  void removeMarker(const QString &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    markers_.erase(
        std::remove_if(markers_.begin(), markers_.end(),
                       [&id](const DiagnosticMarker &m) { return m.id == id; }),
        markers_.end());
  }

  std::vector<DiagnosticMarker> getMarkers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return markers_;
  }

  std::vector<DiagnosticMarker> getMarkersByType(MarkerType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiagnosticMarker> filtered;
    for (const auto &m : markers_) {
      if (m.type == type)
        filtered.push_back(m);
    }
    return filtered;
  }

  void clearMarkers() {
    std::lock_guard<std::mutex> lock(mutex_);
    markers_.clear();
  }

  // Serialization
  QJsonDocument toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    QJsonArray array;
    for (const auto &m : markers_) {
      QJsonObject obj;
      obj["id"] = m.id;
      obj["timestamp"] = m.timestamp.toString(Qt::ISODate);
      obj["type"] = static_cast<int>(m.type);
      obj["message"] = m.message;
      obj["location"] = m.location;
      obj["author"] = m.author;
      array.append(obj);
    }
    QJsonObject root;
    root["markers"] = array;
    return QJsonDocument(root);
  }

  void fromJson(const QJsonDocument &doc) {
    std::lock_guard<std::mutex> lock(mutex_);
    markers_.clear();
    QJsonObject root = doc.object();
    QJsonArray array = root["markers"].toArray();
    for (const auto &val : array) {
      QJsonObject obj = val.toObject();
      DiagnosticMarker m;
      m.id = obj["id"].toString();
      m.timestamp =
          QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
      m.type = static_cast<MarkerType>(obj["type"].toInt());
      m.message = obj["message"].toString();
      m.location = obj["location"].toString();
      m.author = obj["author"].toString();
      markers_.push_back(m);
    }
  }

  bool saveToFile(const QString &filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
      return false;
    file.write(toJson().toJson());
    return true;
  }

  bool loadFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
      return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    fromJson(doc);
    return true;
  }
};

} // namespace ArtifactCore