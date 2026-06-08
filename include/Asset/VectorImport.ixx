module;
#include <QString>
#include <QStringList>
#include <QSize>
#include <QVector>
#include <QFileInfo>
#include <QtCore/QRectF>

export module Asset.VectorImport;

import AssetType;
import File.TypeDetector;

export namespace ArtifactCore {

enum class VectorSourceKind : quint8 {
    Unknown = 0,
    Illustrator,
    Pdf,
    Eps,
    Svg,
    AffinityExport,
};

enum class VectorImportMode : quint8 {
    PreviewOnly = 0,
    EditableAttempted,
    EditablePartial,
    EditableSuccess,
};

struct VectorSourcePageInfo {
    QString name;
    QRectF bounds;
    bool isArtboard = false;
};

struct VectorImportNodeSummary {
    int totalNodes = 0;
    int groupNodes = 0;
    int pathNodes = 0;
    int textNodes = 0;
    int imageNodes = 0;
};

struct VectorImportIssue {
    QString code;
    QString message;
};

inline bool isVectorDocumentType(FileType type)
{
    switch (type) {
    case FileType::Document:
    case FileType::Image:
        return true;
    case FileType::Unknown:
    case FileType::Video:
    case FileType::Audio:
    case FileType::Text:
    case FileType::Binary:
    case FileType::Archive:
    case FileType::Model3D:
    default:
        return false;
    }
}

inline VectorSourceKind vectorSourceKindForExtension(const QString& extension)
{
    const QString ext = extension.trimmed().toLower().startsWith('.')
        ? extension.trimmed().toLower().mid(1)
        : extension.trimmed().toLower();
    if (ext == QStringLiteral("ai")) return VectorSourceKind::Illustrator;
    if (ext == QStringLiteral("pdf")) return VectorSourceKind::Pdf;
    if (ext == QStringLiteral("eps")) return VectorSourceKind::Eps;
    if (ext == QStringLiteral("svg")) return VectorSourceKind::Svg;
    if (ext == QStringLiteral("afdesign") || ext == QStringLiteral("afphoto") || ext == QStringLiteral("afpub")) {
        return VectorSourceKind::AffinityExport;
    }
    return VectorSourceKind::Unknown;
}

struct VectorImportResult {
    QString sourcePath;
    QString displayName;
    AssetType assetType = AssetType::Data;
    FileType fileType = FileType::Unknown;
    VectorSourceKind sourceKind = VectorSourceKind::Unknown;
    VectorImportMode importMode = VectorImportMode::PreviewOnly;
    QSize sourceSize;
    QVector<VectorSourcePageInfo> pages;
    VectorImportNodeSummary nodeSummary;
    QStringList presentFeatures;
    QStringList unsupportedFeatures;
    QVector<VectorImportIssue> issues;
    bool exists = false;
    bool readable = false;
    bool previewReady = false;
    bool editableReady = false;

    bool hasUnsupportedFeatures() const { return !unsupportedFeatures.isEmpty(); }
    bool isPartial() const {
        return importMode == VectorImportMode::EditablePartial ||
               !unsupportedFeatures.isEmpty() || !issues.isEmpty();
    }
};

inline VectorImportResult makeVectorImportResult(const QString& sourcePath)
{
    VectorImportResult result;
    result.sourcePath = sourcePath;

    const QFileInfo info(sourcePath);
    result.exists = info.exists();
    result.readable = info.isReadable();
    result.displayName = info.fileName().isEmpty() ? sourcePath : info.fileName();
    result.sourceKind = vectorSourceKindForExtension(info.suffix());

    if (result.exists) {
        result.assetType = AssetType::Data;
        result.fileType = FileTypeDetector().detect(info.absoluteFilePath());
        result.previewReady = isVectorDocumentType(result.fileType);
        result.editableReady = result.previewReady && result.sourceKind != VectorSourceKind::Unknown;
        result.importMode = result.editableReady ? VectorImportMode::EditableAttempted
                                                 : VectorImportMode::PreviewOnly;
    }

    return result;
}

} // namespace ArtifactCore
