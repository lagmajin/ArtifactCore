module;
#include <QString>

export module Core.AI.OnnxImageSegmenter;

import Core.AI.ImageSegmenter;

export namespace ArtifactCore {

class OnnxImageSegmenter final : public IImageSegmenter {
public:
    OnnxImageSegmenter();
    ~OnnxImageSegmenter() override;
    OnnxImageSegmenter(const OnnxImageSegmenter&) = delete;
    OnnxImageSegmenter& operator=(const OnnxImageSegmenter&) = delete;

    bool initialize(const QString& modelPath);
    bool isInitialized() const noexcept;
    DepthMap segment(const ImageF32x4_RGBA& image,
                     const ImageSegmentationOptions& options = {}) override;
    QString backendName() const override;
    QString lastError() const override;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

}
