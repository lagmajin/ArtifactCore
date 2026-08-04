module;
#include <utility>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QUuid>

module Asset.Importer;

import AssetType;
import Asset.Database;
import File.TypeDetector;
import Asset.VectorImport;

namespace ArtifactCore {

QUuid AssetImporter::importFile(const QString& filePath) {
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return QUuid();
    const QString absolutePath = info.absoluteFilePath();
    if (!isSupported(info.suffix())) return QUuid();

    const AssetType type = detectType(absolutePath);
    if (type == AssetType::Unknown) return QUuid();
    return AssetDatabase::instance().registerAsset(absolutePath, type);
}

bool AssetImporter::isSupported(const QString& extension) {
    QString ext = extension.toLower();
    if (ext.startsWith(".")) ext = ext.mid(1);
    
    // Simple list of supported extensions
    static const QStringList supported = {
        "jpg", "jpeg", "png", "bmp", "gif", "tga", "exr", "hdr", "tif", "tiff",
        "webp", "ico", "dds", "ktx", "psd", "psb",
        "ai", "pdf", "eps", "svg",
        "mp4", "mov", "avi", "mkv",
        "wav", "mp3", "flac", "aac",
        "obj", "fbx", "abc", "glb", "gltf",
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
