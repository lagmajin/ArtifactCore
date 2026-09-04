module;
#include <QString>

export module Core.AI.OnnxImageSegmenter;

import Core.AI.ImageSegmenter;

export namespace ArtifactCore {

enum class OnnxSegmentationOutputActivation {
  None,
  Sigmoid,
  Softmax,
};

enum class OnnxImageInputColorOrder {
  RGB,
  BGR,
};

struct OnnxImageSegmentationOptions {
  // Zero uses the model's declared NCHW input dimensions.
  int inputWidth = 0;
  int inputHeight = 0;
  int outputIndex = 0;
  int foregroundChannel = 0;
  bool preserveAspectRatio = false;
  float inputPaddingValue = 0.0f;
  float inputScale = 1.0f;
  float inputRedMean = 0.0f;
  float inputGreenMean = 0.0f;
  float inputBlueMean = 0.0f;
  float inputRedStdDev = 1.0f;
  float inputGreenStdDev = 1.0f;
  float inputBlueStdDev = 1.0f;
  OnnxImageInputColorOrder inputColorOrder = OnnxImageInputColorOrder::RGB;
  OnnxSegmentationOutputActivation outputActivation =
      OnnxSegmentationOutputActivation::Sigmoid;
  bool preferDirectML = true;
};

struct OnnxImageSegmentationModelInfo {
  bool ready = false;
  bool usingDirectML = false;
  int inputWidth = 0;
  int inputHeight = 0;
  int inputChannels = 0;
  QString inputName;
  QString outputName;
};

// Generic adapter for binary foreground models with NCHW float input and a
// float mask tensor whose spatial dimensions are its final two dimensions.
class OnnxImageSegmenter final : public IImageSegmenter {
public:
  OnnxImageSegmenter();
  ~OnnxImageSegmenter() override;

  OnnxImageSegmenter(const OnnxImageSegmenter&) = delete;
  OnnxImageSegmenter& operator=(const OnnxImageSegmenter&) = delete;

  bool initialize(const QString& modelPath);
  // Load preprocessing and output-selection options before initialize().
  bool loadOptionsFromJson(const QString& configurationPath);
  void reset() noexcept;
  bool isReady() const noexcept override;
  bool segment(const ImageF32x4_RGBA& source,
               DepthMap& foregroundMask) override;

  void setOptions(const OnnxImageSegmentationOptions& options) noexcept;
  OnnxImageSegmentationOptions options() const noexcept;
  OnnxImageSegmentationModelInfo modelInfo() const;
  QString lastError() const override;

private:
  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace ArtifactCore
