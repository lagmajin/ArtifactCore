module;
#include <QString>

export module Core.AI.OnnxImageDepthEstimator;

import Core.AI.ImageDepthEstimator;

export namespace ArtifactCore {

class OnnxImageDepthEstimator final : public IImageDepthEstimator {
public:
    OnnxImageDepthEstimator();
    ~OnnxImageDepthEstimator() override;
    OnnxImageDepthEstimator(const OnnxImageDepthEstimator&) = delete;
    OnnxImageDepthEstimator& operator=(const OnnxImageDepthEstimator&) = delete;

    bool initialize(const QString& modelPath);
    bool isInitialized() const noexcept;
    DepthMap estimate(const ImageF32x4_RGBA& image,
                      const ImageDepthEstimateOptions& options = {}) override;
    QString backendName() const override;
    QString lastError() const override;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

}
