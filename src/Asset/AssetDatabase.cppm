module;
#include <utility>
#include <QString>
#include <QUuid>
#include <QMap>
#include <QList>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QIODevice>

module Asset.Database;

namespace ArtifactCore {

QUuid AssetDatabase::registerAsset(const QString& path, AssetType type) {
    if (pathToId_.contains(path)) {
        return pathToId_[path];
    }

    QUuid id = QUuid::createUuid();
    AssetInfo info;
    info.id = id;
    info.absolutePath = path;
    info.name = QFileInfo(path).fileName();
    info.type = type;

    assets_[id] = info;
    pathToId_[path] = id;
    
    return id;
}

void AssetDatabase::unregisterAsset(const QUuid& id) {
    if (assets_.contains(id)) {
        pathToId_.remove(assets_[id].absolutePath);
        assets_.remove(id);
    }
}

AssetInfo AssetDatabase::getAssetInfo(const QUuid& id) const {
    return assets_.value(id);
}

QUuid AssetDatabase::findAssetByPath(const QString& path) const {
    return pathToId_.value(path);
}

QList<AssetInfo> AssetDatabase::findAssetsByType(AssetType type) const {
    QList<AssetInfo> result;
    for (const auto& info : assets_) {
        if (info.type == type) result.append(info);
    }
    return result;
}

QList<AssetInfo> AssetDatabase::allAssets() const {
    return assets_.values();
}

bool AssetDatabase::load(const QString& databasePath) {
    QFile file(databasePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray array = doc.array();

    assets_.clear();
    pathToId_.clear();

    for (const auto& v : array) {
        QJsonObject obj = v.toObject();
        AssetInfo info;
        info.id = QUuid::fromString(obj["id"].toString());
        info.name = obj["name"].toString();
        info.absolutePath = obj["path"].toString();
        info.type = static_cast<AssetType>(obj["type"].toInt());
        
        assets_[info.id] = info;
        pathToId_[info.absolutePath] = info.id;
    }
    return true;
}

bool AssetDatabase::save(const QString& databasePath) const {
    QJsonArray array;
    for (const auto& info : assets_) {
        QJsonObject obj;
        obj["id"] = info.id.toString();
        obj["name"] = info.name;
        obj["path"] = info.absolutePath;
        obj["type"] = static_cast<int>(info.type);
        array.append(obj);
    }

    QJsonDocument doc(array);
    QFile file(databasePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    return true;
}

} // namespace ArtifactCore
