module;
#include <vector>
#include <QString>

export module Core.AI.RotoBrushImageSegmenter;

import Core.AI.ImageSegmenter;
import ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;

export namespace ArtifactCore {

// Interactive, model-free segmentation fallback. Strokes are supplied in
// source-image pixels and are retained until replaced or cleared.
class RotoBrushImageSegmenter final : public IImageSegmenter {
public:
  RotoBrushImageSegmenter();
  ~RotoBrushImageSegmenter() override;

  RotoBrushImageSegmenter(const RotoBrushImageSegmenter&) = delete;
  RotoBrushImageSegmenter& operator=(const RotoBrushImageSegmenter&) = delete;

  bool isReady() const noexcept override;
  bool segment(const ImageF32x4_RGBA& source, DepthMap& foregroundMask) override;
  // Call segment() for the base frame before propagating to subsequent frames.
  bool propagateToNextFrame(const ImageF32x4_RGBA& previousFrame,
                            const ImageF32x4_RGBA& currentFrame,
                            DepthMap& foregroundMask);
  QString lastError() const override;

  void setStrokes(const std::vector<RotoBrushStroke>& strokes);
  const std::vector<RotoBrushStroke>& strokes() const noexcept;
  void clearStrokes();

private:
  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace ArtifactCore
