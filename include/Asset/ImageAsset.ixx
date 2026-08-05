module;
#include <QString>

export module ImageAsset;

import Asset.File;

export namespace ArtifactCore {

class ImageAssetFile final : public AbstractAssetFile {
public:
    struct Meta {
        int width = 0;
        int height = 0;
        int channelCount = 0;
        int bitDepth = 0;
        QString pixelType;
        QString colorSpace;
        bool hdr = false;
    };

    explicit ImageAssetFile(const QString& path);
    const Meta& imageMeta() const { return imageMeta_; }
    bool refreshMetadata();
    bool isImageMetadataValid() const {
        return imageMeta_.width > 0 && imageMeta_.height > 0;
    }

protected:
    bool _load() override;

private:
    Meta imageMeta_;
};

}
