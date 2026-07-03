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
import Image.MultiChannelImage;

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
    QString tempName = info.completeBaseName();
    if (tempName.isEmpty()) {
        tempName = baseName;
    }
    tempName += ".tmp." + stamp + "." + QString::number(salt);
    const QString suffix = info.suffix();
    if (!suffix.isEmpty()) {
        tempName += "." + suffix;
    }
    return info.dir().filePath(tempName);
}

OIIO::TypeDesc resolveWriteType(const QString& filePath, const ImageExportOptions& options)
{
    OIIO::TypeDesc type = options.dataType;
    if (type == OIIO::TypeDesc::UNKNOWN) {
        type = OIIO::TypeDesc::UINT8;
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("exr") && type == OIIO::TypeDesc::UINT8) {
        type = OIIO::TypeDesc::FLOAT;
    }
    if (type == OIIO::TypeDesc::HALF) {
        type = OIIO::TypeDesc::FLOAT;
    }
    return type;
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
    const TypeDesc writeType = resolveWriteType(filePath, options);
    spec.format = writeType;

    if (!out->open(filePath.toStdString(), spec)) {
        return makeError("encode.open", "Could not open output: " + QString::fromStdString(out->geterror()));
    }

    std::vector<float> pixelsF32;
    const void* writeData = nullptr;

    if (writeType != TypeDesc::UINT8) {
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
    const TypeDesc writeType = resolveWriteType(filePath, options);
    spec.format = writeType;

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

    if (!out->write_image(writeType, packed.data())) {
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

namespace {
QString channelTypeToOIIOName(ChannelType type) {
    switch (type) {
    case ChannelType::Red: return QStringLiteral("R");
    case ChannelType::Green: return QStringLiteral("G");
    case ChannelType::Blue: return QStringLiteral("B");
    case ChannelType::Alpha: return QStringLiteral("A");
    case ChannelType::Depth: return QStringLiteral("Depth");
    case ChannelType::NormalX: return QStringLiteral("Normal.X");
    case ChannelType::NormalY: return QStringLiteral("Normal.Y");
    case ChannelType::NormalZ: return QStringLiteral("Normal.Z");
    case ChannelType::VelocityX: return QStringLiteral("Velocity.X");
    case ChannelType::VelocityY: return QStringLiteral("Velocity.Y");
    case ChannelType::ObjectId: return QStringLiteral("ObjectId");
    case ChannelType::MaterialId: return QStringLiteral("MaterialId");
    case ChannelType::AlbedoR: return QStringLiteral("Albedo.R");
    case ChannelType::AlbedoG: return QStringLiteral("Albedo.G");
    case ChannelType::AlbedoB: return QStringLiteral("Albedo.B");
    case ChannelType::Emission: return QStringLiteral("Emission");
    case ChannelType::Custom: return QStringLiteral("Custom");
    default: return QStringLiteral("Unknown");
    }
}

void applyStringAttributes(OIIO::ImageSpec& spec,
                           const ArtifactCore::ImageExportOptions& options) {
    if (!options.creator.trimmed().isEmpty()) {
        spec.attribute("Software", options.creator.toStdString());
    }
    if (!options.copyright.trimmed().isEmpty()) {
        spec.attribute("Copyright", options.copyright.toStdString());
    }
    for (auto it = options.stringAttributes.constBegin();
         it != options.stringAttributes.constEnd(); ++it) {
        if (it.key().trimmed().isEmpty()) {
            continue;
        }
        spec.attribute(it.key().toStdString(), it.value().toStdString());
    }
}

bool hasCryptomatteLayer(const ArtifactCore::ImageExportOptions& options,
                         const QString& standardId)
{
    const QString prefix = QStringLiteral("cryptomatte/%1/").arg(standardId);
    return options.stringAttributes.contains(prefix + QStringLiteral("name")) &&
           options.stringAttributes.contains(prefix + QStringLiteral("manifest"));
}
} // namespace

ImageExportResult ImageExporter::writeMultiChannel(const MultiChannelImage& multiImage, const QString& filePath, const ImageExportOptions& options)
{
    using namespace OIIO;
    if (multiImage.isEmpty()) {
        return makeError("validate", "MultiChannelImage is empty.");
    }
    const int width = multiImage.width();
    const int height = multiImage.height();
    const size_t pixelCount = static_cast<size_t>(width) * height;

    // Collect enabled channels with their OIIO names
    const auto types = multiImage.channelTypes();
    std::vector<QString> channelNames;
    std::vector<std::shared_ptr<const VideoChannel>> channelData;
    std::vector<int> syntheticChannelIndices;
    for (const auto& type : types) {
        auto ch = multiImage.getChannel(type);
        if (!ch || !ch->data()) continue;
        channelNames.push_back(channelTypeToOIIOName(type));
        channelData.push_back(ch);
        syntheticChannelIndices.push_back(-1);
    }

    std::vector<std::vector<float>> syntheticStorage;
    auto appendDraftCryptomatteChannels =
        [&](ChannelType type, const QString& standardId, const QString& layerName) {
            if (!hasCryptomatteLayer(options, standardId)) {
                return;
            }
            auto sourceChannel = multiImage.getChannel(type);
            if (!sourceChannel || !sourceChannel->data() || sourceChannel->size() == 0) {
                return;
            }

            constexpr const char* kSuffixes[] = {"R", "G", "B", "A"};
            for (int component = 0; component < 4; ++component) {
                syntheticStorage.emplace_back(pixelCount, 0.0f);
                const int syntheticIndex =
                    static_cast<int>(syntheticStorage.size()) - 1;
                auto& dst = syntheticStorage.back();
                for (std::size_t i = 0; i < pixelCount; ++i) {
                    if (component == 0) {
                        dst[i] = sourceChannel->data()[i];
                    } else if (component == 1) {
                        dst[i] = sourceChannel->data()[i] > 0.0f ? 1.0f : 0.0f;
                    }
                }
                channelNames.push_back(
                    QStringLiteral("%1.%2")
                        .arg(layerName, QString::fromLatin1(kSuffixes[component])));
                channelData.push_back(std::shared_ptr<const VideoChannel>());
                syntheticChannelIndices.push_back(syntheticIndex);
            }
        };
    appendDraftCryptomatteChannels(ChannelType::ObjectId,
                                   QStringLiteral("00000000"),
                                   QStringLiteral("CryptoObject00"));
    appendDraftCryptomatteChannels(ChannelType::MaterialId,
                                   QStringLiteral("00000001"),
                                   QStringLiteral("CryptoMaterial00"));

    if (channelNames.size() < 3) {
        return makeError("validate", "Need at least 3 channels for multi-channel export.");
    }
    const int nch = static_cast<int>(channelNames.size());

    // Create ImageSpec with named channels
    const TypeDesc writeType = resolveWriteType(filePath, options);
    ImageSpec spec(width, height, nch, writeType);
    spec.channelnames.clear();
    for (int i = 0; i < nch; ++i) {
        spec.channelnames.push_back(channelNames[i].toStdString());
    }
    // Apply compression etc from options
    if (!options.compression.isEmpty()) {
        spec.attribute("compression", options.compression.toStdString());
    }
    if (options.compressionQuality >= 0) {
        spec.attribute("CompressionQuality", options.compressionQuality);
    }
    spec.set_colorspace(options.colorSpace.toStdString());
    applyStringAttributes(spec, options);

    // Build interleaved pixel buffer
    std::vector<float> interleaved(pixelCount * nch, 0.0f);
    for (size_t p = 0; p < pixelCount; ++p) {
        for (int c = 0; c < nch; ++c) {
            if (channelData[c]) {
                interleaved[p * nch + c] = channelData[c]->data()[p];
                continue;
            }

            const int syntheticIndex = syntheticChannelIndices[c];
            if (syntheticIndex >= 0 &&
                syntheticIndex < static_cast<int>(syntheticStorage.size())) {
                interleaved[p * nch + c] = syntheticStorage[syntheticIndex][p];
            }
        }
    }

    // Wrap in ImageBuf and write
    ImageBuf imageBuf(spec, interleaved.data());
    return encodeImageBufToPath(imageBuf, filePath, options);
}

std::future<ImageExportResult> ImageExporter::writeMultiChannelAsync(const MultiChannelImage& multiImage, const QString& filePath, const ImageExportOptions& options)
{
    auto promise = std::make_shared<std::promise<ImageExportResult>>();
    std::future<ImageExportResult> future = promise->get_future();
    auto imageCopy = std::make_shared<MultiChannelImage>(multiImage);

    std::thread([this, imageCopy, filePath, options, promise]() {
        ImageExportResult result = writeMultiChannel(*imageCopy, filePath, options);
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
