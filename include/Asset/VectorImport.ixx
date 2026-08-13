module;
#include <QString>
#include <QStringList>
#include <QSize>
#include <QVector>
#include <QFileInfo>
#include <QtCore/QRectF>
#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <cmath>

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
        return true;
    case FileType::Image:
        return false;
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

inline VectorImportResult makeVectorImportResult(const QString& sourcePath);

class VectorAssetFile {
public:
    explicit VectorAssetFile(const QString& sourcePath)
        : sourcePath_(sourcePath), result_(makeVectorImportResult(sourcePath)) {}

    const QString& sourcePath() const { return sourcePath_; }
    const VectorImportResult& importResult() const { return result_; }
    VectorSourceKind sourceKind() const { return result_.sourceKind; }
    bool isReadable() const { return result_.readable; }
    bool isEditable() const { return result_.editableReady; }

private:
    QString sourcePath_;
    VectorImportResult result_;
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

        if (result.sourceKind == VectorSourceKind::Svg && result.readable) {
            QFile svgFile(info.absoluteFilePath());
            if (svgFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QXmlStreamReader xml(&svgFile);
                while (!xml.atEnd()) {
                    xml.readNext();
                    if (!xml.isStartElement()) continue;
                    const QString name = xml.name().toString().toLower();
                    if (name == QStringLiteral("svg")) {
                        const auto attrs = xml.attributes();
                        const QString viewBox = attrs.value(QStringLiteral("viewBox")).toString();
                        const QStringList parts = viewBox.split(QRegularExpression(QStringLiteral("\\s+|,")), Qt::SkipEmptyParts);
                        if (parts.size() == 4) {
                            bool okW = false, okH = false;
                            const double width = parts[2].toDouble(&okW);
                            const double height = parts[3].toDouble(&okH);
                            if (okW && okH && width > 0.0 && height > 0.0) {
                                result.sourceSize = QSize(static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
                                result.pages.push_back({result.displayName, QRectF(0.0, 0.0, width, height), true});
                            }
                        }
                    } else if (name == QStringLiteral("g")) {
                        ++result.nodeSummary.groupNodes;
                    } else if (name == QStringLiteral("path")) {
                        ++result.nodeSummary.pathNodes;
                    } else if (name == QStringLiteral("text")) {
                        ++result.nodeSummary.textNodes;
                    } else if (name == QStringLiteral("image")) {
                        ++result.nodeSummary.imageNodes;
                    } else if (name == QStringLiteral("filter") || name == QStringLiteral("mask") ||
                               name == QStringLiteral("pattern") || name == QStringLiteral("foreignobject")) {
                        result.unsupportedFeatures.push_back(name);
                    }
                }
                if (xml.hasError()) {
                    result.issues.push_back({QStringLiteral("svg.parse"), xml.errorString()});
                    result.previewReady = false;
                }
                result.nodeSummary.totalNodes = result.nodeSummary.groupNodes +
                    result.nodeSummary.pathNodes + result.nodeSummary.textNodes + result.nodeSummary.imageNodes;
                result.editableReady = result.previewReady && result.nodeSummary.pathNodes > 0;
                result.importMode = result.editableReady && !result.hasUnsupportedFeatures()
                    ? VectorImportMode::EditableSuccess : VectorImportMode::EditablePartial;
            }
        }
    }

    return result;
}

} // namespace ArtifactCore
