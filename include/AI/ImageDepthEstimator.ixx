module;
#include <memory>
#include <QString>

export module Core.AI.ImageDepthEstimator;

import Image.DepthMap;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct ImageDepthEstimateOptions {
    bool normalize = true;
    bool invert = false;
    int maxDimension = 1024;
};

class IImageDepthEstimator {
public:
    virtual ~IImageDepthEstimator() = default;
    virtual DepthMap estimate(const ImageF32x4_RGBA& image,
                              const ImageDepthEstimateOptions& options = {}) = 0;
    virtual QString backendName() const = 0;
    virtual QString lastError() const = 0;
};

// Deterministic CPU fallback. This is an approximate luminance-derived depth
// field, not a monocular-AI prediction; it keeps the depth workflow usable
// when no model provider is installed.
class LuminanceImageDepthEstimator final : public IImageDepthEstimator {
public:
    DepthMap estimate(const ImageF32x4_RGBA& image,
                      const ImageDepthEstimateOptions& options = {}) override;
    QString backendName() const override { return QStringLiteral("luminance-fallback"); }
    QString lastError() const override { return {}; }
};

using ImageDepthEstimatorPtr = std::shared_ptr<IImageDepthEstimator>;

}
