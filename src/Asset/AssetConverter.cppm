module;
#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

module AssetConverter;

namespace ArtifactCore {
namespace {
QString oiioError(const OIIO::ImageBuf& image) {
    return QString::fromUtf8(image.geterror().c_str());
}
}

ArtifactAssetConverter::ConversionResult
ArtifactAssetConverter::convert(const ConversionJob& job) const {
    ConversionResult result;
    result.outputPath = job.outputPath;
    const qint64 started = QDateTime::currentMSecsSinceEpoch();
    if (job.sourcePath.isEmpty() || job.outputPath.isEmpty()) {
        result.errorMessage = QStringLiteral("Source and output paths are required");
        return result;
    }
    OIIO::ImageBuf source(job.sourcePath.toUtf8().constData());
    if (!source.read()) {
        result.errorMessage = oiioError(source);
        return result;
    }
    OIIO::ImageBuf output = source;
    if (job.maxResolution.isValid() &&
        (source.spec().width > job.maxResolution.width() ||
         source.spec().height > job.maxResolution.height())) {
        const double scale = std::min(
            static_cast<double>(job.maxResolution.width()) / source.spec().width,
            static_cast<double>(job.maxResolution.height()) / source.spec().height);
        output = OIIO::ImageBufAlgo::resize(
            source, std::max(1, static_cast<int>(source.spec().width * scale)),
            std::max(1, static_cast<int>(source.spec().height * scale)));
    }
    QDir().mkpath(QFileInfo(job.outputPath).absolutePath());
    OIIO::ImageSpec spec = output.spec();
    if (job.jpegQuality > 0 && job.jpegQuality <= 100)
        spec.attribute("jpeg:Quality", job.jpegQuality);
    if (!output.write(job.outputPath.toUtf8().constData(), spec)) {
        result.errorMessage = oiioError(output);
        return result;
    }
    result.success = true;
    result.outputSizeBytes = QFileInfo(job.outputPath).size();
    result.durationMs = QDateTime::currentMSecsSinceEpoch() - started;
    return result;
}

QVector<ArtifactAssetConverter::ConversionResult>
ArtifactAssetConverter::convertBatch(const QVector<ConversionJob>& jobs) const {
    QVector<ConversionResult> results;
    results.reserve(jobs.size());
    for (const auto& job : jobs) results.push_back(convert(job));
    return results;
}

ArtifactAssetConverter::ConversionResult
ArtifactAssetConverter::generateProxy(const QString& sourcePath, int maxWidth,
                                      int maxHeight, const QString& proxyDirectory) const {
    const QFileInfo sourceInfo(sourcePath);
    ConversionJob job;
    job.sourcePath = sourcePath;
    job.outputPath = QDir(proxyDirectory).filePath(
        sourceInfo.completeBaseName() + QStringLiteral("_proxy.jpg"));
    job.targetFormat = QStringLiteral("jpeg");
    job.maxResolution = QSize(maxWidth, maxHeight);
    return convert(job);
}
}
