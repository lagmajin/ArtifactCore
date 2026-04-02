module;
#include <QString>
#include <optional>

export module IO.ImageImporter;

import Image.Raw;
import Image.Import.OIIO;

export namespace ArtifactCore {

struct ImportedImage {
    RawImage rawImage;
    std::optional<ImageMetadata> metadata;  // Present if OIIO path succeeded
    bool usedOIIO = false;                  // Track which path was used
};

class ImageImporter {
public:
    ImageImporter();
    ~ImageImporter();

    ImageImporter(const ImageImporter&) = delete;
    ImageImporter& operator=(const ImageImporter&) = delete;

    bool open(const QString& filePath);
    ImportedImage readImage();  // Returns RawImage + optional metadata
    void close();

    // Explicitly try OIIO path (returns nullopt if OIIO fails)
    static std::optional<ImportedImage> tryOIIO(const QString& filePath);

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
