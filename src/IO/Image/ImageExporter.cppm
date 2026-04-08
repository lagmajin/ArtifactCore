module;
#include <utility>
#include <QImage>
#include <QString>
#include <QFuture>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QRandomGenerator>
#include <QObject>
#include <wobjectimpl.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <algorithm>

module IO.ImageExporter;

import Image.ExportOptions;
import Image.Utils;

namespace ArtifactCore {

namespace {

ImageExportResult makeError(const QString& stage, const QString& message)
{
    ImageExportResult result;
    result.success = false;
    result.errorStage = stage;
    result.errorMessage = message;
    return result;
}

QString makeTemporaryPathNear(const QString& filePath)
{
    QFileInfo info(filePath);
    QString baseName = info.fileName();
    if (baseName.isEmpty()) {
        baseName = "export_image";
    }
    const QString stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    const quint32 salt = QRandomGenerator::global()->generate();
    return info.dir().filePath(baseName + ".tmp." + stamp + "." + QString::number(salt) + ".part");
}

ImageExportResult prepareImageForEncoding(const QImage& src, const ImageExportOptions& options, QImage& converted)
{
    if (src.isNull()) {
        return makeError("prepare", "Source image is null.");
    }

    converted = src.convertToFormat(QImage::Format_RGBA8888);
    if (converted.isNull()) {
        return makeError("prepare", "Failed to convert image to RGBA8888.");
    }

    return ImageExportResult{true, {}, {}};
}

ImageExportResult encodeImageToPath(const QImage& imageRGBA8,
                                    const QString& filePath,
                                    const ImageExportOptions& options)
{
    using namespace OIIO;
    std::unique_ptr<ImageOutput> out = ImageOutput::create(filePath.toStdString());
    if (!out) {
        return makeError("encode.create", "Could not create ImageOutput for: " + filePath);
    }

    ImageSpec spec = makeSpec(imageRGBA8.width(), imageRGBA8.height(), options);
    spec.nchannels = 4;
    spec.channelnames = {"R", "G", "B", "A"};
    spec.format = TypeDesc::UINT8;

    if (!out->open(filePath.toStdString(), spec)) {
        return makeError("encode.open", "Could not open output: " + QString::fromStdString(out->geterror()));
    }

    std::vector<float> pixelsF32;
    TypeDesc writeType = TypeDesc::UINT8;
    const void* writeData = nullptr;

    if (options.dataType == TypeDesc::FLOAT || options.dataType == TypeDesc::HALF) {
        writeType = options.dataType;
        pixelsF32.resize(static_cast<std::size_t>(imageRGBA8.width()) * static_cast<std::size_t>(imageRGBA8.height()) * 4);

        const auto* src = imageRGBA8.constBits();
        const std::size_t pixelCount = static_cast<std::size_t>(imageRGBA8.width()) * static_cast<std::size_t>(imageRGBA8.height());
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::size_t si = i * 4;
            const std::size_t di = i * 4;
            pixelsF32[di + 0] = static_cast<float>(src[si + 0]) / 255.0f;
            pixelsF32[di + 1] = static_cast<float>(src[si + 1]) / 255.0f;
            pixelsF32[di + 2] = static_cast<float>(src[si + 2]) / 255.0f;
            pixelsF32[di + 3] = static_cast<float>(src[si + 3]) / 255.0f;
        }
        writeData = pixelsF32.data();
    } else {
        writeType = TypeDesc::UINT8;
        writeData = imageRGBA8.constBits();
    }

    if (!out->write_image(writeType, writeData)) {
        const QString err = QString::fromStdString(out->geterror());
        out->close();
        return makeError("encode.write", "Could not write image: " + err);
    }

    if (!out->close()) {
        return makeError("encode.close", "Failed to close output: " + QString::fromStdString(out->geterror()));
    }

    return ImageExportResult{true, {}, {}};
}

ImageExportResult encodeImageBufToPath(const OIIO::ImageBuf& imageBuf,
                                       const QString& filePath,
                                       const ImageExportOptions& options)
{
    using namespace OIIO;

    const ImageSpec& inSpec = imageBuf.spec();
    if (inSpec.width <= 0 || inSpec.height <= 0) {
        return makeError("prepare", "Input ImageBuf is empty.");
    }
    if (inSpec.nchannels < 3) {
        return makeError("prepare", "Input ImageBuf must have at least 3 channels.");
    }

    std::unique_ptr<ImageOutput> out = ImageOutput::create(filePath.toStdString());
    if (!out) {
        return makeError("encode.create", "Could not create ImageOutput for: " + filePath);
    }

    ImageSpec spec = makeSpec(inSpec.width, inSpec.height, options);
    spec.nchannels = std::min(4, std::max(3, inSpec.nchannels));
    spec.channelnames = (spec.nchannels == 4)
        ? std::vector<std::string>{"R", "G", "B", "A"}
        : std::vector<std::string>{"R", "G", "B"};
    spec.format = options.dataType;

    if (!out->open(filePath.toStdString(), spec)) {
        return makeError("encode.open", "Could not open output: " + QString::fromStdString(out->geterror()));
    }

    const std::size_t pixelCount = static_cast<std::size_t>(spec.width) * static_cast<std::size_t>(spec.height);
    const int channels = spec.nchannels;

    const std::size_t bytesPerPixel = static_cast<std::size_t>(channels) * static_cast<std::size_t>(spec.format.size());
    std::vector<unsigned char> packed;
    packed.resize(pixelCount * bytesPerPixel);

    if (!imageBuf.get_pixels(ROI::All(), spec.format, packed.data())) {
        out->close();
        return makeError("encode.readpixels", "Failed to read pixels from ImageBuf.");
    }

    if (!out->write_image(spec.format, packed.data())) {
        const QString err = QString::fromStdString(out->geterror());
        out->close();
        return makeError("encode.write", "Could not write image: " + err);
    }

    if (!out->close()) {
        return makeError("encode.close", "Failed to close output: " + QString::fromStdString(out->geterror()));
    }

    return ImageExportResult{true, {}, {}};
}

ImageExportResult commitAtomically(const QString& tempPath, const QString& finalPath)
{
    QFileInfo finalInfo(finalPath);
    QDir dir = finalInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return makeError("commit.mkdir", "Failed to create output directory: " + dir.absolutePath());
    }

    if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
        return makeError("commit.remove", "Failed to replace existing file: " + finalPath);
    }

    if (!QFile::rename(tempPath, finalPath)) {
        const QString why = QFile(tempPath).errorString();
        return makeError("commit.rename", "Atomic rename failed from '" + tempPath + "' to '" + finalPath + "': " + why);
    }

    return ImageExportResult{true, {}, {}};
}

} // namespace

class ImageExporter::Impl {
public:
    Impl() {}
    ~Impl() {}
};

W_OBJECT_IMPL(ImageExporter)

ImageExporter::ImageExporter(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>())
{
}

ImageExporter::~ImageExporter()
{
}

ImageExportResult ImageExporter::write(const QImage& image, const QString& filePath, const ImageExportOptions& options)
{
    if (filePath.isEmpty()) {
        return makeError("validate", "Output path is empty.");
    }

    QImage converted;
    ImageExportResult prep = prepareImageForEncoding(image, options, converted);
    if (!prep.success) {
        return prep;
    }

    const QString tempPath = makeTemporaryPathNear(filePath);
    ImageExportResult encoded = encodeImageToPath(converted, tempPath, options);
    if (!encoded.success) {
        QFile::remove(tempPath);
        return encoded;
    }

    ImageExportResult committed = commitAtomically(tempPath, filePath);
    if (!committed.success) {
        QFile::remove(tempPath);
        return committed;
    }

    return ImageExportResult{true, {}, {}};
}

ImageExportResult ImageExporter::write(const OIIO::ImageBuf& image, const QString& filePath, const ImageExportOptions& options)
{
    if (filePath.isEmpty()) {
        return makeError("validate", "Output path is empty.");
    }

    const QString tempPath = makeTemporaryPathNear(filePath);
    ImageExportResult encoded = encodeImageBufToPath(image, tempPath, options);
    if (!encoded.success) {
        QFile::remove(tempPath);
        return encoded;
    }

    ImageExportResult committed = commitAtomically(tempPath, filePath);
    if (!committed.success) {
        QFile::remove(tempPath);
        return committed;
    }

    return ImageExportResult{true, {}, {}};
}

std::future<ImageExportResult> ImageExporter::writeAsync(const OIIO::ImageBuf& image, const QString& filePath, const ImageExportOptions& options)
{
    auto promise = std::make_shared<std::promise<ImageExportResult>>();
    std::future<ImageExportResult> future = promise->get_future();
    auto imageCopy = std::make_shared<OIIO::ImageBuf>();
    imageCopy->copy(image);

    std::thread([this, imageCopy, filePath, options, promise]() {
        ImageExportResult result = write(*imageCopy, filePath, options);
        promise->set_value(result);
    }).detach();

    return future;
}

std::future<ImageExportResult> ImageExporter::writeAsync(const QImage& image, const QString& filePath, const ImageExportOptions& options)
{
    auto promise = std::make_shared<std::promise<ImageExportResult>>();
    std::future<ImageExportResult> future = promise->get_future();

    std::thread([this, image, filePath, options, promise]() {
        ImageExportResult result = write(image, filePath, options);
        promise->set_value(result);
    }).detach();

    return future;
}

ImageExportResult ImageExporter::testWrite()
{
    return write(QImage(100, 100, QImage::Format_RGBA8888), "test_output.png", ImageExportOptions());
}

ImageExportResult ImageExporter::testWrite2()
{
    ImageExportResult result;
    result.success = true;
    return result;
}

}
