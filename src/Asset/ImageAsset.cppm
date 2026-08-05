module;
#include <string>
#include <QString>

module ImageAsset;

import IO.ImageImporter;

namespace ArtifactCore {

ImageAssetFile::ImageAssetFile(const QString& path)
    : AbstractAssetFile(path.toStdString()) {}

bool ImageAssetFile::refreshMetadata() {
    const QString path = filePath().toQString();
    ImageImporter importer;
    if (!importer.open(path)) return false;
    const RawImage image = importer.readImage();
    importer.close();
    if (!image.isValid()) return false;
    imageMeta_.width = image.width;
    imageMeta_.height = image.height;
    imageMeta_.channelCount = image.channels;
    imageMeta_.bitDepth = image.bitsPerChannel;
    imageMeta_.pixelType = image.pixelType;
    imageMeta_.colorSpace = image.colorSpace;
    imageMeta_.hdr = image.isHDR;
    meta().setValue(UniString("width"), UniString(std::to_string(image.width)));
    meta().setValue(UniString("height"), UniString(std::to_string(image.height)));
    meta().setValue(UniString("channels"), UniString(std::to_string(image.channels)));
    meta().setValue(UniString("bitDepth"), UniString(std::to_string(image.bitsPerChannel)));
    return true;
}

bool ImageAssetFile::_load() { return refreshMetadata(); }

}
