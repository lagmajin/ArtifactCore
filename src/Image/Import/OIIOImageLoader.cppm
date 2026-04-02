module;
#include <QDebug>
#include <QImage>
#include <QSize>
#include <QColor>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <Image/Import/OIIOImageLoader.ixx>

module Image.Import.OIIO;

import Image.Raw;
import std;

namespace ArtifactCore {

namespace {

OIIO::ImageSpec getDefaultSpec() {
    OIIO::ImageSpec spec;
    spec.format = OIIO::TypeDesc::UINT8;
    spec.nchannels = 4;
    spec.channelnames = {"R", "G", "B", "A"};
    return spec;
}

} // anonymous namespace

std::optional<ImageImportResult> loadImageWithOIIO(const QString& filePath) {
    OIIO::ImageBuf buf;
    OIIO::ImageSpec config;
    config.format = OIIO::TypeDesc::UINT8; // Request 8-bit per channel
    config.nchannels = 0; // Let OIIO decide (preserve original)

    std::string pathStr = filePath.toStdString();

    try {
        // OIIO read with auto-orient (EXIF rotation)
        OIIO::ImageBuf::ReadResult result = OIIO::ImageBuf::read(pathStr, nullptr, &config);
        if (!result || !result->initialized()) {
            qWarning() << "[OIIOImageLoader] Failed to read image:" << filePath;
            return std::nullopt;
        }

        buf = std::move(*result);

        ImageImportResult importResult;

        // Extract metadata
        importResult.metadata.width = buf.spec().width;
        importResult.metadata.height = buf.spec().height;
        importResult.metadata.channels = buf.spec().nchannels;
        importResult.metadata.pixelType = buf.spec().format.c_str();

        // Color space: OIIO stores this in "oiio:Colorspace" or similar
        importResult.metadata.colorspace = extractColorSpace(buf);

        // Copy all attributes
        for (const auto& attr : buf.spec().extra_attribs) {
            importResult.metadata.attributes[attr.name().c_str()] = attr.get_string();
        }

        // Convert to QImage for UI compatibility
        importResult.image = convertImageBufToQImage(buf);

        if (!importResult.image || importResult.image->isNull()) {
            qWarning() << "[OIIOImageLoader] QImage conversion failed:" << filePath;
            return std::nullopt;
        }

        // Also create RawImage for processing pipeline
        importResult.rawImage = convertImageBufToRaw(buf);

        qDebug() << "[OIIOImageLoader] Successfully loaded:" << filePath
                 << "size=" << buf.spec().width << "x" << buf.spec().height
                 << "channels=" << buf.spec().nchannels
                 << "colorspace=" << importResult.metadata.colorspace;

        return importResult;

    } catch (const std::exception& e) {
        qWarning() << "[OIIOImageLoader] Exception while loading" << filePath << ":" << e.what();
        return std::nullopt;
    }
}

std::string extractColorSpace(const OIIO::ImageBuf& buf) {
    // Common OIIO color space attributes
    static const std::vector<std::string> colorSpaceAttrs = {
        "oiio:Colorspace",
        "ColorSpace",
        "colorspace"
    };

    for (const auto& attrName : colorSpaceAttrs) {
        if (buf.spec().find_attribute(attrName)) {
            return buf.spec().get_string(attrName);
        }
    }

    // Try to infer from pixel type/channels as fallback
    if (buf.spec().nchannels >= 3) {
        return "sRGB"; // Assume sRGB for RGB images
    }
    return "Unknown";
}

std::shared_ptr<QImage> convertImageBufToQImage(const OIIO::ImageBuf& buf) {
    if (!buf.initialized()) {
        return nullptr;
    }

    const OIIO::ImageSpec& spec = buf.spec();
    QImage::Format qFormat = QImage::Format_Invalid;

    // Determine QImage format from OIIO spec
    if (spec.nchannels == 1) {
        qFormat = QImage::Format_Grayscale8;
    } else if (spec.nchannels == 3) {
        if (spec.format == OIIO::TypeDesc::UINT8) {
            qFormat = QImage::Format_RGB888;
        } else if (spec.format == OIIO::TypeDesc::FLOAT) {
            // OIIO doesn't have Format_RGB32F directly, handle later
            qFormat = QImage::Format_RGB888; // fallback for now
        }
    } else if (spec.nchannels == 4) {
        if (spec.format == OIIO::TypeDesc::UINT8) {
            qFormat = QImage::Format_RGBA8888;
        } else if (spec.format == OIIO::TypeDesc::FLOAT) {
            qFormat = QImage::Format_RGBA32F; // May need conversion
        }
    }

    if (qFormat == QImage::Format_Invalid) {
        // Try to force to RGBA8888 via OIIO conversion
        OIIO::ImageBuf convBuf = OIIO::ImageBufAlgo::flip(buf, true); // triggers conversion
        // This is simplified; full conversion needs proper channel format mapping
        qWarning() << "[OIIOImageLoader] Unsupported format, using fallback";
        return nullptr;
    }

    // Create QImage from buffer
    const uint8_t* pixelData = static_cast<const uint8_t*>(buf.localpixels());
    if (!pixelData) {
        return nullptr;
    }

    QImage img(pixelData, spec.width, spec.height, static_cast<int>(buf.pixel_bytes()), qFormat);

    if (img.isNull()) {
        return nullptr;
    }

    // Deep copy since OIIO buffer may be freed
    return std::make_shared<QImage>(img.copy());
}

ArtifactCore::RawImage convertImageBufToRaw(const OIIO::ImageBuf& buf) {
    ArtifactCore::RawImage raw;

    raw.width = buf.spec().width;
    raw.height = buf.spec().height;
    raw.channels = buf.spec().nchannels;
    raw.pixelType = QString::fromStdString(buf.spec().format.c_str());

    const size_t totalSize = buf.pixel_bytes() * buf.spec().width * buf.spec().height;
    raw.data.resize(static_cast<int>(totalSize));

    if (buf.localpixels()) {
        std::memcpy(raw.data.data(), buf.localpixels(), totalSize);
    } else {
        // Allocate and read
        OIIO::ImageBuf localCopy = buf; // This creates a copy
        std::memcpy(raw.data.data(), localCopy.localpixels(), totalSize);
    }

    return raw;
}

} // namespace ArtifactCore
