module;
#include <utility>
#include <cstring>
#include <array>
#include <QFileInfo>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

#include <stb_image.h>

module IO.ImageImporter;

import Image.PSDDocument;

namespace ArtifactCore {

class ImageImporter::Impl {
public:
    QString filePath;
    bool open = false;
    bool psd = false;
    PsdDocument psdDocument;

    void reset()
    {
        filePath.clear();
        open = false;
        psd = false;
        psdDocument.close();
    }
};

namespace {

OIIO::TypeDesc chooseReadType(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("exr")) {
        return OIIO::TypeDesc::FLOAT;
    }
    return OIIO::TypeDesc::UINT8;
}

RawImage loadRawImageViaOIIO(const QString& filePath)
{
    RawImage image;
    if (filePath.isEmpty()) {
        return image;
    }

    const std::string utf8Path = filePath.toUtf8().toStdString();
    OIIO::ImageBuf source(utf8Path);
    const OIIO::TypeDesc readType = chooseReadType(filePath);
    if (!source.read(0, 0, true, readType)) {
        return image;
    }

    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
        return image;
    }

    OIIO::ImageBuf rgba;
    if (spec.nchannels >= 4) {
        const std::array<int, 4> channelOrder{0, 1, 2, 3};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);
    } else if (spec.nchannels == 3) {
        const std::array<int, 4> channelOrder{0, 1, 2, -1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else if (spec.nchannels == 2) {
        const std::array<int, 4> channelOrder{0, 0, 0, 1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else {
        const std::array<int, 4> channelOrder{0, 0, 0, -1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    }

    image.width = spec.width;
    image.height = spec.height;
    image.channels = 4;
    if (readType == OIIO::TypeDesc::FLOAT) {
        image.pixelType = QStringLiteral("float");
        image.data.resize(spec.width * spec.height * 4 * static_cast<int>(sizeof(float)));
    } else {
        image.pixelType = QStringLiteral("uint8");
        image.data.resize(spec.width * spec.height * 4);
    }
    if (!rgba.get_pixels(OIIO::ROI::All(), readType, image.data.data())) {
        return {};
    }
    return image;
}

} // namespace

ImageImporter::ImageImporter()
    : impl_(new Impl())
{
}

ImageImporter::~ImageImporter()
{
    delete impl_;
}

bool ImageImporter::open(const QString& filePath)
{
    if (!impl_) {
        return false;
    }
    impl_->reset();
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
    impl_->filePath = filePath;
    impl_->open = true;
    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("psd") || suffix == QStringLiteral("psb")) {
        impl_->psd = impl_->psdDocument.open(filePath);
    }
    return true;
}

RawImage ImageImporter::readImage()
{
    if (!impl_ || !impl_->open || impl_->filePath.isEmpty()) {
        return {};
    }

    if (impl_->psd && impl_->psdDocument.isOpen()) {
        RawImage preview = impl_->psdDocument.flattenedPreview();
        if (preview.isValid()) {
            return preview;
        }
    }

    if (RawImage image = loadRawImageViaOIIO(impl_->filePath); image.isValid()) {
        return image;
    }

    const QByteArray encoded = QFile::encodeName(impl_->filePath);
    int w = 0;
    int h = 0;
    int comp = 0;

    stbi_uc* pixels = stbi_load(encoded.constData(), &w, &h, &comp, 4);
    if (pixels) {
        RawImage image;
        image.width = w;
        image.height = h;
        image.channels = 4;
        image.pixelType = QStringLiteral("uint8");
        image.data.resize(static_cast<int>(w * h * 4));
        std::memcpy(image.data.data(), pixels, image.data.size());
        stbi_image_free(pixels);
        return image;
    }

    if (stbi_is_16_bit(encoded.constData())) {
        stbi_us* pixels16 = stbi_load_16(encoded.constData(), &w, &h, &comp, 4);
        if (pixels16) {
            RawImage image;
            image.width = w;
            image.height = h;
            image.channels = 4;
            image.pixelType = QStringLiteral("uint16");
            image.data.resize(static_cast<int>(w * h * 4 * static_cast<int>(sizeof(stbi_us))));
            std::memcpy(image.data.data(), pixels16, image.data.size());
            stbi_image_free(pixels16);
            return image;
        }
    }

    return {};
}

void ImageImporter::close()
{
    if (impl_) {
        impl_->reset();
    }
}

} // namespace ArtifactCore
