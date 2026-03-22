module;
#include <QString>
#include <QFileInfo>
#include <QUuid>

module Asset.Importer;

import AssetType;
import Asset.Database;
import File.TypeDetector;

namespace ArtifactCore {

QUuid AssetImporter::importFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists()) return QUuid();

    AssetType type = detectType(filePath);
    return AssetDatabase::instance().registerAsset(filePath, type);
}

bool AssetImporter::isSupported(const QString& extension) {
    QString ext = extension.toLower();
    if (ext.startsWith(".")) ext = ext.mid(1);
    
    // Simple list of supported extensions
    static const QStringList supported = {
        "jpg", "jpeg", "png", "tga", "exr", "tif", "tiff",
        "mp4", "mov", "avi", "mkv",
        "wav", "mp3", "flac", "aac",
        "obj", "fbx", "abc", "glb", "gltf"
    };
    
    return supported.contains(ext);
}

AssetType AssetImporter::detectType(const QString& filePath) {
    FileTypeDetector detector;
    FileType ft = detector.detect(filePath);
    
    switch (ft) {
        case FileType::Image: return AssetType::Image;
        case FileType::Video: return AssetType::Video;
        case FileType::Audio: return AssetType::Audio;
        case FileType::Model3D: return AssetType::Model;
        default: return AssetType::Unknown;
    }
}

} // namespace ArtifactCore
