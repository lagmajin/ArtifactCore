module;
#include <utility>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

module Asset;

namespace ArtifactCore {

ArtifactAssetMetaFile::ArtifactAssetMetaFile(QString assetPath)
    : assetPath_(std::move(assetPath)) {}

QString ArtifactAssetMetaFile::metaPathFor(const QString& assetPath) {
    return assetPath + QStringLiteral(".assetmeta");
}

ArtifactAssetMetaFile ArtifactAssetMetaFile::load(const QString& assetPath) {
    ArtifactAssetMetaFile result(assetPath);
    QFile file(metaPathFor(assetPath));
    if (!file.open(QIODevice::ReadOnly)) return result;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (document.isObject()) result.data_ = document.object();
    return result;
}

ArtifactAssetMetaFile ArtifactAssetMetaFile::fromJson(const QByteArray& json,
                                                       const QString& assetPath) {
    ArtifactAssetMetaFile result(assetPath);
    const auto document = QJsonDocument::fromJson(json);
    if (document.isObject()) result.data_ = document.object();
    return result;
}

bool ArtifactAssetMetaFile::isValid() const {
    return !data_.isEmpty() && data_.value(QStringLiteral("version")).toInt() > 0;
}

bool ArtifactAssetMetaFile::save() const {
    if (assetPath_.isEmpty()) return false;
    QSaveFile file(metaPathFor(assetPath_));
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray payload = QJsonDocument(data_).toJson(QJsonDocument::Indented);
    return file.write(payload) == payload.size() && file.commit();
}

QJsonObject ArtifactAssetMetaFile::toJson() const { return data_; }

QString ArtifactAssetMetaFile::sourcePath() const {
    return data_.value(QStringLiteral("asset")).toObject()
        .value(QStringLiteral("sourcePath")).toString();
}

QUuid ArtifactAssetMetaFile::uuid() const {
    return QUuid(data_.value(QStringLiteral("asset")).toObject()
                     .value(QStringLiteral("uuid")).toString());
}

AssetType ArtifactAssetMetaFile::type() const {
    return assetTypeFromName(data_.value(QStringLiteral("asset")).toObject()
                                 .value(QStringLiteral("type")).toString());
}

QDateTime ArtifactAssetMetaFile::importedAt() const {
    return QDateTime::fromString(data_.value(QStringLiteral("asset")).toObject()
                                     .value(QStringLiteral("importedAt")).toString(),
                                 Qt::ISODate);
}

void ArtifactAssetMetaFile::setUuid(const QUuid& value) {
    auto asset = data_.value(QStringLiteral("asset")).toObject();
    asset.insert(QStringLiteral("uuid"), value.toString(QUuid::WithoutBraces));
    data_.insert(QStringLiteral("asset"), asset);
    data_.insert(QStringLiteral("version"), 1);
}

void ArtifactAssetMetaFile::setType(AssetType value) {
    auto asset = data_.value(QStringLiteral("asset")).toObject();
    asset.insert(QStringLiteral("type"), assetTypeName(value));
    data_.insert(QStringLiteral("asset"), asset);
    data_.insert(QStringLiteral("version"), 1);
}

void ArtifactAssetMetaFile::setImportedAt(const QDateTime& value) {
    auto asset = data_.value(QStringLiteral("asset")).toObject();
    asset.insert(QStringLiteral("importedAt"), value.toUTC().toString(Qt::ISODate));
    data_.insert(QStringLiteral("asset"), asset);
    data_.insert(QStringLiteral("version"), 1);
}

QStringList ArtifactAssetMetaFile::proxyResolutions() const {
    return data_.value(QStringLiteral("proxies")).toObject().keys();
}

QString ArtifactAssetMetaFile::proxyPath(const QString& resolution) const {
    return data_.value(QStringLiteral("proxies")).toObject()
        .value(resolution).toString();
}

void ArtifactAssetMetaFile::addProxy(const QString& resolution, const QString& path) {
    auto proxies = data_.value(QStringLiteral("proxies")).toObject();
    proxies.insert(resolution, path);
    data_.insert(QStringLiteral("proxies"), proxies);
    data_.insert(QStringLiteral("version"), 1);
}

QStringList ArtifactAssetMetaFile::tags() const {
    QStringList result;
    for (const auto& value : data_.value(QStringLiteral("tags")).toArray())
        result.push_back(value.toString());
    return result;
}

void ArtifactAssetMetaFile::addTag(const QString& tag) {
    const QString normalized = tag.trimmed();
    if (normalized.isEmpty()) return;
    auto values = data_.value(QStringLiteral("tags")).toArray();
    if (!tags().contains(normalized)) values.append(normalized);
    data_.insert(QStringLiteral("tags"), values);
    data_.insert(QStringLiteral("version"), 1);
}

void ArtifactAssetMetaFile::removeTag(const QString& tag) {
    QJsonArray values;
    for (const auto& value : data_.value(QStringLiteral("tags")).toArray())
        if (value.toString() != tag) values.append(value);
    data_.insert(QStringLiteral("tags"), values);
}

QVariant ArtifactAssetMetaFile::customValue(const QString& key) const {
    return data_.value(QStringLiteral("custom")).toObject().value(key).toVariant();
}

void ArtifactAssetMetaFile::setCustomValue(const QString& key, const QVariant& value) {
    auto custom = data_.value(QStringLiteral("custom")).toObject();
    custom.insert(key, QJsonValue::fromVariant(value));
    data_.insert(QStringLiteral("custom"), custom);
    data_.insert(QStringLiteral("version"), 1);
}

QString ArtifactAssetMetaFile::assetTypeName(AssetType type) {
    switch (type) {
    case AssetType::Image: return QStringLiteral("Image");
    case AssetType::Audio: return QStringLiteral("Audio");
    case AssetType::Video: return QStringLiteral("Video");
    case AssetType::Model: return QStringLiteral("Model");
    case AssetType::Script: return QStringLiteral("Script");
    case AssetType::Folder: return QStringLiteral("Folder");
    case AssetType::Data: return QStringLiteral("Data");
    default: return QStringLiteral("Unknown");
    }
}

AssetType ArtifactAssetMetaFile::assetTypeFromName(const QString& name) {
    const auto value = name.trimmed().toLower();
    if (value == QStringLiteral("image")) return AssetType::Image;
    if (value == QStringLiteral("audio")) return AssetType::Audio;
    if (value == QStringLiteral("video")) return AssetType::Video;
    if (value == QStringLiteral("model")) return AssetType::Model;
    if (value == QStringLiteral("script")) return AssetType::Script;
    if (value == QStringLiteral("folder")) return AssetType::Folder;
    if (value == QStringLiteral("data")) return AssetType::Data;
    return AssetType::Unknown;
}

}


