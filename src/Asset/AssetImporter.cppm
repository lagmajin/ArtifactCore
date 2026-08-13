module;
#include <algorithm>
#include <utility>
#include <QString>
#include <QSize>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>

module Asset.Importer;

import AssetType;
import Asset.Database;
import File.TypeDetector;
import Asset.VectorImport;
import Asset;
import ImageAsset;
import AssetConverter;
import Configuration.LayeredConfigStore;

namespace ArtifactCore {

QUuid AssetImporter::importFile(const QString& filePath) {
    AssetImportSettings settings;
    auto& config = LayeredConfigStore::instance();
    settings.generateProxyOnImport = config.valueBool(
        QStringLiteral("Asset/GenerateProxyOnImport"), settings.generateProxyOnImport);
    settings.proxyResolution = QSize(
        std::max(1, static_cast<int>(config.valueInt64(
            QStringLiteral("Asset/ProxyWidth"), settings.proxyResolution.width()))),
        std::max(1, static_cast<int>(config.valueInt64(
            QStringLiteral("Asset/ProxyHeight"), settings.proxyResolution.height()))));
    settings.jpegQuality = std::clamp(
        static_cast<int>(config.valueInt64(QStringLiteral("Asset/ProxyJpegQuality"),
                                           settings.jpegQuality)), 1, 100);
    return importFile(filePath, settings);
}

QUuid AssetImporter::importFile(const QString& filePath,
                                const AssetImportSettings& settings) {
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return QUuid();
    const QString absolutePath = info.absoluteFilePath();
    if (!isSupported(info.suffix())) return QUuid();

    const AssetType type = detectType(absolutePath);
    if (type == AssetType::Unknown) return QUuid();
    const VectorSourceKind vectorKind =
        vectorSourceKindForExtension(info.suffix());
    const bool isVector = vectorKind != VectorSourceKind::Unknown;
    if (type == AssetType::Image) {
        ImageAssetFile imageAsset(absolutePath);
        imageAsset.refreshMetadata();
    }
    const QUuid id = AssetDatabase::instance().registerAsset(absolutePath, type);
    if (id.isNull()) return id;

    auto meta = ArtifactAssetMetaFile::load(absolutePath);
    if (!meta.isValid()) {
        meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"), absolutePath);
        meta.setUuid(id);
        meta.setType(type);
        meta.setImportedAt(QDateTime::currentDateTimeUtc());
        auto asset = meta.toJson();
        auto assetObject = asset.value(QStringLiteral("asset")).toObject();
        assetObject.insert(QStringLiteral("sourcePath"), absolutePath);
        asset.insert(QStringLiteral("asset"), assetObject);
        meta = ArtifactAssetMetaFile::fromJson(
            QJsonDocument(asset).toJson(QJsonDocument::Compact), absolutePath);
    }
    if (settings.generateProxyOnImport && type == AssetType::Image) {
        ArtifactAssetConverter converter;
        const QString proxyDir = QDir(info.absolutePath()).filePath(
            QStringLiteral(".artifact/proxies"));
        const auto proxy = converter.generateProxy(
            absolutePath, settings.proxyResolution.width(),
            settings.proxyResolution.height(), proxyDir);
        if (proxy.success) {
            meta.addProxy(QStringLiteral("configured"), proxy.outputPath);
        }
    }
    if (type == AssetType::Image) {
        ImageAssetFile imageAsset(absolutePath);
        if (imageAsset.refreshMetadata()) {
            const auto& image = imageAsset.imageMeta();
            meta.setCustomValue(QStringLiteral("image/width"), image.width);
            meta.setCustomValue(QStringLiteral("image/height"), image.height);
            meta.setCustomValue(QStringLiteral("image/channels"), image.channelCount);
            meta.setCustomValue(QStringLiteral("image/bitDepth"), image.bitDepth);
            meta.setCustomValue(QStringLiteral("image/pixelType"), image.pixelType);
            meta.setCustomValue(QStringLiteral("image/colorSpace"), image.colorSpace);
            meta.setCustomValue(QStringLiteral("image/hdr"), image.hdr);
        }
    }
    if (isVector) {
        const VectorAssetFile vectorAsset(absolutePath);
        const auto& result = vectorAsset.importResult();
        meta.setCustomValue(QStringLiteral("vector/sourceKind"),
                            static_cast<int>(result.sourceKind));
        meta.setCustomValue(QStringLiteral("vector/readable"), result.readable);
        meta.setCustomValue(QStringLiteral("vector/editable"), result.editableReady);
        meta.setCustomValue(QStringLiteral("vector/importMode"),
                            static_cast<int>(result.importMode));
    }
    meta.save();
    return id;
}

bool AssetImporter::isSupported(const QString& extension) {
    QString ext = extension.toLower();
    if (ext.startsWith(".")) ext = ext.mid(1);
    
    // Simple list of supported extensions
    static const QStringList supported = {
        "jpg", "jpeg", "png", "bmp", "gif", "tga", "exr", "hdr", "tif", "tiff",
        "webp", "ico", "dds", "ktx", "ktx2", "avif", "heic", "heif",
        "jxl", "jp2", "j2k", "ppm", "pgm", "pbm", "pam", "pfm",
        "psd", "psb",
        "ai", "pdf", "eps", "svg", "afdesign", "afphoto", "afpub",
        "mp4", "mov", "avi", "mkv",
        "wav", "mp3", "flac", "aac",
        "obj", "fbx", "abc", "glb", "gltf", "stl",
        "json"
    };
    
    return supported.contains(ext);
}

AssetType AssetImporter::detectType(const QString& filePath) {
    FileTypeDetector detector;
    FileType ft = detector.detect(filePath);
    const VectorSourceKind vectorKind = vectorSourceKindForExtension(QFileInfo(filePath).suffix());
    
    if (vectorKind != VectorSourceKind::Unknown || ft == FileType::Document ||
        ft == FileType::Font) {
        return AssetType::Data;
    }
    if (ft == FileType::Text) {
        return AssetType::Data;
    }

    switch (ft) {
        case FileType::Image: return AssetType::Image;
        case FileType::Video: return AssetType::Video;
        case FileType::Audio: return AssetType::Audio;
        case FileType::Model3D: return AssetType::Model;
        default: return AssetType::Unknown;
    }
}

} // namespace ArtifactCore
