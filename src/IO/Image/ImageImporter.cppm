module;
#include <QFileInfo>
#include <QFile>
#include <QByteArray>
#include <cstring>

#include <stb_image.h>

module IO.ImageImporter;

import Image.PSDDocument;
import Image.Import.OIIO;
import std;

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

ImportedImage ImageImporter::readImage()
{
    ImportedImage result;

    if (!impl_ || !impl_->open || impl_->filePath.isEmpty()) {
        return result;
    }

    // First try OIIO path for metadata preservation
    auto oiiResult = ArtifactCore::loadImageWithOIIO(impl_->filePath);
    if (oiiResult.has_value()) {
        const auto& import = oiiResult.value();
        result.rawImage = import.rawImage;
        result.metadata = import.metadata;
        result.usedOIIO = true;
        return result;
    }

    // Fallback to stb_image path (existing behavior)
    if (impl_->psd && impl_->psdDocument.isOpen()) {
        RawImage preview = impl_->psdDocument.flattenedPreview();
        if (preview.isValid()) {
            result.rawImage = preview;
            return result;
        }
    }

    const QByteArray encoded = QFile::encodeName(impl_->filePath);
    int w = 0;
    int h = 0;
    int comp = 0;

    stbi_uc* pixels = stbi_load(encoded.constData(), &w, &h, &comp, 4);
    if (pixels) {
        result.rawImage.width = w;
        result.rawImage.height = h;
        result.rawImage.channels = 4;
        result.rawImage.pixelType = QStringLiteral("uint8");
        result.rawImage.data.resize(static_cast<int>(w * h * 4));
        std::memcpy(result.rawImage.data.data(), pixels, result.rawImage.data.size());
        stbi_image_free(pixels);
        return result;
    }

    if (stbi_is_16_bit(encoded.constData())) {
        stbi_us* pixels16 = stbi_load_16(encoded.constData(), &w, &h, &comp, 4);
        if (pixels16) {
            result.rawImage.width = w;
            result.rawImage.height = h;
            result.rawImage.channels = 4;
            result.rawImage.pixelType = QStringLiteral("uint16");
            result.rawImage.data.resize(static_cast<int>(w * h * 4 * static_cast<int>(sizeof(stbi_us))));
            std::memcpy(result.rawImage.data.data(), pixels16, result.rawImage.data.size());
            stbi_image_free(pixels16);
            return result;
        }
    }

    return result;
}

std::optional<ImportedImage> ImageImporter::tryOIIO(const QString& filePath)
{
    auto oiioResult = ArtifactCore::loadImageWithOIIO(filePath);
    if (!oiioResult.has_value()) {
        return std::nullopt;
    }

    ImportedImage result;
    result.rawImage = oiioResult->rawImage;
    result.metadata = oiioResult->metadata;
    result.usedOIIO = true;
    return result;
}

void ImageImporter::close()
{
    if (impl_) {
        impl_->reset();
    }
}

} // namespace ArtifactCore
