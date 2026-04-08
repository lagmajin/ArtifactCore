module;
#include <utility>
#include <cstring>
#include <QFileInfo>
#include <QFile>
#include <QByteArray>
#include <QString>

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
