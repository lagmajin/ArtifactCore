module;
#include <algorithm>
#include <cstdint>
#include <QString>

export module Core.AI.ImageAIProcessing;

export namespace ArtifactCore {

// Shared vocabulary for every local or remote image-AI backend.  Individual
// feature modules expose their typed inputs and outputs separately.
enum class ImageAICapability : std::uint32_t {
  Segmentation = 1u << 0,
  ObjectDetection = 1u << 1,
  TextRecognition = 1u << 2,
  SuperResolution = 1u << 3,
  ImageRestoration = 1u << 4,
  DepthEstimation = 1u << 5,
  SubjectTracking = 1u << 6,
  ImageGeneration = 1u << 7,
  Inpainting = 1u << 8,
  ImageUnderstanding = 1u << 9,
  SafetyClassification = 1u << 10,
};

using ImageAICapabilityMask = std::uint32_t;

constexpr ImageAICapabilityMask imageAICapabilityMask(ImageAICapability capability) noexcept {
  return static_cast<ImageAICapabilityMask>(capability);
}

struct ImageAIModelDescriptor {
  QString identifier;
  QString displayName;
  QString modelPath;
  QString configurationPath;
  ImageAICapabilityMask capabilities = 0;
  bool local = true;
};

struct ImageAIExecutionOptions {
  int maximumInputWidth = 0;
  int maximumInputHeight = 0;
  bool preferGpu = true;
  bool allowFallback = true;
};

struct ImageAIDiagnostics {
  QString backend;
  QString modelIdentifier;
  QString error;
  double elapsedMilliseconds = 0.0;
  bool usedGpu = false;

  bool succeeded() const noexcept { return error.isEmpty(); }
};

inline bool imageAIModelSupports(const ImageAIModelDescriptor& model,
                                 ImageAICapability capability) noexcept {
  return (model.capabilities & imageAICapabilityMask(capability)) != 0;
}

} // namespace ArtifactCore
