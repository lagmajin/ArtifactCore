module;
class tst_QList;
#include <utility>
#include <QString>
#include <QUuid>
#include <QMap>
#include <QList>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QIODevice>

module Asset.Database;

namespace ArtifactCore {

namespace {
QString normalizedAssetPath(const QString& path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QFileInfo info(trimmed);
    QString normalized = info.canonicalFilePath();
    if (normalized.isEmpty()) {
        normalized = info.absoluteFilePath();
    }
    normalized = QDir::cleanPath(normalized);
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}
}

QUuid AssetDatabase::registerAsset(const QString& path, AssetType type) {
    return registerAsset(path, type, {});
}

QUuid AssetDatabase::registerAsset(const QString& path, AssetType type,
                                   const QUuid& preferredId) {
    const int rawType = static_cast<int>(type);
    if (rawType <= static_cast<int>(AssetType::Unknown) ||
        rawType > static_cast<int>(AssetType::Data)) {
        return {};
    }
    const QString identity = normalizedAssetPath(path);
    if (identity.isEmpty()) {
        return {};
    }
    if (pathToId_.contains(identity)) {
        return pathToId_[identity];
    }

    QUuid id = preferredId.isNull() ? QUuid::createUuid() : preferredId;
    if (assets_.contains(id) && assets_[id].absolutePath != identity) {
        id = QUuid::createUuid();
    }
    AssetInfo info;
    info.id = id;
    info.absolutePath = identity;
    info.name = QFileInfo(identity).fileName();
    info.type = type;

    assets_[id] = info;
    pathToId_[identity] = id;
    
    return id;
}

void AssetDatabase::unregisterAsset(const QUuid& id) {
    if (assets_.contains(id)) {
        const QString path = assets_[id].absolutePath;
        if (pathToId_.value(path) == id) {
            pathToId_.remove(path);
        }
        assets_.remove(id);
    }
}

AssetInfo AssetDatabase::getAssetInfo(const QUuid& id) const {
    return assets_.value(id);
}

QUuid AssetDatabase::findAssetByPath(const QString& path) const {
    return pathToId_.value(normalizedAssetPath(path));
}

bool AssetDatabase::relinkAssetPath(const QString& oldPath,
                                    const QString& newPath) {
    const QString oldIdentity = normalizedAssetPath(oldPath);
    const QString newIdentity = normalizedAssetPath(newPath);
    if (oldIdentity.isEmpty() || newIdentity.isEmpty() ||
        oldIdentity == newIdentity) {
        return false;
    }
    const auto oldIt = pathToId_.constFind(oldIdentity);
    if (oldIt == pathToId_.cend() || pathToId_.contains(newIdentity)) {
        return false;
    }
    const QUuid assetId = oldIt.value();
    auto assetIt = assets_.find(assetId);
    if (assetIt == assets_.end()) {
        return false;
    }
    pathToId_.remove(oldIdentity);
    assetIt->absolutePath = newIdentity;
    assetIt->name = QFileInfo(newIdentity).fileName();
    pathToId_.insert(newIdentity, assetId);
    return true;
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
    if (doc.isNull() || !doc.isArray()) return false;
    QJsonArray array = doc.array();

    assets_.clear();
    pathToId_.clear();

    for (const auto& v : array) {
        QJsonObject obj = v.toObject();
        AssetInfo info;
        info.id = QUuid::fromString(obj["id"].toString());
        info.name = obj["name"].toString();
        info.absolutePath = normalizedAssetPath(obj["path"].toString());
        const int rawType = obj["type"].toInt(static_cast<int>(AssetType::Unknown));
        if (rawType <= static_cast<int>(AssetType::Unknown) ||
            rawType > static_cast<int>(AssetType::Data)) {
            continue;
        }
        info.type = static_cast<AssetType>(rawType);
        const QJsonObject metadata = obj.value(QStringLiteral("metadata")).toObject();
        for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            if (it.value().isString()) {
                info.metadata.insert(it.key(), it.value().toString());
            }
        }
        
        if (info.id.isNull() || info.absolutePath.isEmpty()) {
            continue;
        }
        if (info.name.trimmed().isEmpty()) {
            info.name = QFileInfo(info.absolutePath).fileName();
        }
        if (assets_.contains(info.id)) {
            continue;
        }
        const auto existingId = pathToId_.value(info.absolutePath);
        if (!existingId.isNull()) {
            continue;
        }
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
        QJsonObject metadata;
        for (auto it = info.metadata.cbegin(); it != info.metadata.cend(); ++it) {
            metadata.insert(it.key(), it.value());
        }
        obj["metadata"] = metadata;
        array.append(obj);
    }

    QJsonDocument doc(array);
    QSaveFile file(databasePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    if (file.write(doc.toJson(QJsonDocument::Compact)) < 0) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace ArtifactCore
