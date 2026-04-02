module;
#include <QImage>
#include <QString>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <map>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/version.h>

export module Image.Import.OIIO;

import Image.Raw;

export namespace ArtifactCore {

struct ImageMetadata {
    QImage::Format format = QImage::Format_Invalid;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string colorspace;  // Input colorspace from file
    std::string pixelType;   // OIIO pixel type (e.g., "uint8", "float")
    std::map<std::string, std::string> attributes;  // Additional metadata (EXIF, XMP, etc.)
};

struct ImageImportResult {
    // Loaded image data in QImage format (for UI compatibility)
    std::shared_ptr<QImage> image;

    // Raw pixel buffer for processing pipeline (using project RawImage)
    ArtifactCore::RawImage rawImage;

    // Image metadata
    ImageMetadata metadata;

    bool isValid() const { return image && !image->isNull(); }
};

// Load image using OIIO, preserving metadata
// Returns empty result if OIIO fails or format not supported
std::optional<ImageImportResult> loadImageWithOIIO(const QString& filePath);

// Try to extract color space from OIIO ImageBuf
std::string extractColorSpace(const OIIO::ImageBuf& buf);

// Convert OIIO ImageBuf to QImage (suitable for UI display)
std::shared_ptr<QImage> convertImageBufToQImage(const OIIO::ImageBuf& buf);

// Convert OIIO ImageBuf to RawBuffer (for processing pipeline)
ArtifactCore::RawImage convertImageBufToRaw(const OIIO::ImageBuf& buf);

} // namespace ArtifactCore
