module;
#include <QString>
#include <QSize>
#include <QVector>

export module AssetConverter;

export namespace ArtifactCore {
class ArtifactAssetConverter {
public:
    struct ConversionJob {
        QString sourcePath;
        QString outputPath;
        QString targetFormat;
        QSize maxResolution;
        int jpegQuality = 85;
        bool normalizeColorSpace = false;
    };
    struct ConversionResult {
        bool success = false;
        QString outputPath;
        QString errorMessage;
        qint64 outputSizeBytes = 0;
        double durationMs = 0.0;
    };
    ConversionResult convert(const ConversionJob& job) const;
    QVector<ConversionResult> convertBatch(const QVector<ConversionJob>& jobs) const;
    ConversionResult generateProxy(const QString& sourcePath, int maxWidth,
                                   int maxHeight, const QString& proxyDirectory) const;
};
}
