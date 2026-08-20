module;
#include <memory>
#include <QString>

export module Core.AI.ImageSegmenter;

import Image.DepthMap;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct ImageSegmentationOptions {
    bool normalize = true;
    bool invert = false;
    bool applySigmoid = false;
    float threshold = 0.5f;
    int maxDimension = 1024;
};

class IImageSegmenter {
public:
    virtual ~IImageSegmenter() = default;
    virtual DepthMap segment(const ImageF32x4_RGBA& image,
                             const ImageSegmentationOptions& options = {}) = 0;
    virtual QString backendName() const = 0;
    virtual QString lastError() const = 0;
};

class LuminanceImageSegmenter final : public IImageSegmenter {
public:
    DepthMap segment(const ImageF32x4_RGBA& image,
                     const ImageSegmentationOptions& options = {}) override;
    QString backendName() const override { return QStringLiteral("luminance-fallback"); }
    QString lastError() const override { return {}; }
};

using ImageSegmenterPtr = std::shared_ptr<IImageSegmenter>;

// Applies a normalized segmentation mask to image alpha in-place. The mask
// is sampled bilinearly so lower-resolution model output remains usable.
void applySegmentationMask(ImageF32x4_RGBA& image, const DepthMap& mask,
                           float opacity = 1.0f);

}
