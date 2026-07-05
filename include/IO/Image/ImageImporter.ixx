module;
#include <utility>
#include <QString>

export module IO.ImageImporter;

import Image.Raw;

export namespace ArtifactCore {

class ImageImporter {
public:
    ImageImporter();
    ~ImageImporter();

    ImageImporter(const ImageImporter&) = delete;
    ImageImporter& operator=(const ImageImporter&) = delete;

    bool open(const QString& filePath);
    RawImage readImage();
    void close();

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
