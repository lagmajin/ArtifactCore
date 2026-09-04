module;
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>
#include <QString>

export module Core.AI.ImageSegmenter;
import Image.DepthMap;
import Image.ImageF32x4_RGBA;
import FloatRGBA;

export namespace ArtifactCore {

enum class SegmentationMaskMode {
  MultiplyAlpha,
  ReplaceAlpha,
};

struct SegmentationMaskOptions {
  // 0 keeps the original alpha; 1 applies the generated mask fully.
  float opacity = 1.0f;
  // A zero threshold preserves the model's soft output. Values above zero
  // isolate the foreground at that confidence.
  float threshold = 0.0f;
  // Width of the soft transition centered on threshold, in mask-value units.
  float softness = 0.0f;
  bool invert = false;
  SegmentationMaskMode mode = SegmentationMaskMode::MultiplyAlpha;
};

enum class SegmentationSpillChannel {
  Green,
  Blue,
};

// Color-key spill correction is intentionally limited to soft matte edges;
// applying it to fully opaque foreground would alter the subject's colors.
struct SegmentationDespillOptions {
  float strength = 0.0f;
  float edgeWidth = 0.25f;
  SegmentationSpillChannel channel = SegmentationSpillChannel::Green;
};

// Model implementations own inference only. They return a normalized, single
// channel foreground mask, leaving all image mutation to this shared contract.
class IImageSegmenter {
public:
  virtual ~IImageSegmenter() = default;
  virtual bool isReady() const noexcept = 0;
  virtual bool segment(const ImageF32x4_RGBA& source,
                       DepthMap& foregroundMask) = 0;
  virtual QString lastError() const = 0;
};

// Deliberately non-AI fallback for installations without a local inference
// model. It is useful for high-contrast product shots, but must never be
// presented as a person or object segmentation result.
struct LuminanceSegmentationOptions {
  float blackPoint = 0.05f;
  float whitePoint = 0.95f;
  float gamma = 1.0f;
  bool darkForeground = false;
  bool respectSourceAlpha = true;
};

class LuminanceImageSegmenter final : public IImageSegmenter {
public:
  explicit LuminanceImageSegmenter(
      const LuminanceSegmentationOptions& options = {}) noexcept
      : options_(options) {}

  bool isReady() const noexcept override { return true; }

  QString lastError() const override { return {}; }

  void setOptions(const LuminanceSegmentationOptions& options) noexcept {
    options_ = options;
  }

  const LuminanceSegmentationOptions& options() const noexcept {
    return options_;
  }

  bool segment(const ImageF32x4_RGBA& source,
               DepthMap& foregroundMask) override {
    if (source.isEmpty()) {
      foregroundMask.clear();
      return false;
    }

    const float blackPoint = std::clamp(options_.blackPoint, 0.0f, 1.0f);
    const float whitePoint = std::max(
        std::clamp(options_.whitePoint, 0.0f, 1.0f), blackPoint + 0.0001f);
    const float gamma = std::max(options_.gamma, 0.0001f);
    foregroundMask.resize(source.width(), source.height());

    for (int y = 0; y < source.height(); ++y) {
      for (int x = 0; x < source.width(); ++x) {
        const auto pixel = source.getPixel(x, y);
        const float luma = std::clamp(
            pixel.r() * 0.2126f + pixel.g() * 0.7152f + pixel.b() * 0.0722f,
            0.0f, 1.0f);
        float foreground = std::clamp(
            (luma - blackPoint) / (whitePoint - blackPoint), 0.0f, 1.0f);
        foreground = std::pow(foreground, 1.0f / gamma);
        if (options_.darkForeground) { foreground = 1.0f - foreground; }
        if (options_.respectSourceAlpha) {
          foreground *= std::clamp(pixel.a(), 0.0f, 1.0f);
        }
        foregroundMask.setValue(x, y, foreground);
      }
    }
    return true;
  }

private:
  LuminanceSegmentationOptions options_;
};

// Post-inference operations stay in Core so every model backend produces the
// same usable matte. Positive expandPixels grows foreground; negative values
// contract it. Feathering is intentionally a small box blur, not a substitute
// for model-aware hair matting.
struct SegmentationMaskRefinementOptions {
  float threshold = 0.0f;
  float softness = 0.0f;
  int expandPixels = 0;
  bool fillHoles = false;
  int maximumHoleArea = 0;
  int maximumSmallComponentArea = 0;
  int featherPixels = 0;
};

struct SegmentationHoleFillOptions {
  float backgroundThreshold = 0.5f;
  // Zero accepts every enclosed hole; a positive value limits removal to
  // small background islands measured in mask pixels.
  int maximumArea = 0;
};

struct SegmentationSmallComponentOptions {
  float foregroundThreshold = 0.5f;
  int maximumArea = 0;
};

// Fills only background components that do not touch the image boundary.
// This avoids turning genuine exterior background into foreground.
inline bool fillSegmentationMaskHoles(
    DepthMap& mask, const SegmentationHoleFillOptions& options = {}) {
  if (mask.isEmpty()) { return false; }
  const int width = mask.width();
  const int height = mask.height();
  const float threshold = std::clamp(options.backgroundThreshold, 0.0f, 1.0f);
  const int maximumArea = std::max(options.maximumArea, 0);
  std::vector<bool> visited(static_cast<size_t>(width) * height, false);
  std::vector<int> pending;
  std::vector<int> component;
  pending.reserve(static_cast<size_t>(width) * height / 8 + 1);
  component.reserve(static_cast<size_t>(width) * height / 8 + 1);
  for (int startY = 0; startY < height; ++startY) {
    for (int startX = 0; startX < width; ++startX) {
      const int start = startY * width + startX;
      if (visited[static_cast<size_t>(start)] || mask.value(startX, startY) >= threshold) {
        continue;
      }
      pending.clear();
      component.clear();
      pending.push_back(start);
      visited[static_cast<size_t>(start)] = true;
      bool touchesBoundary = false;
      while (!pending.empty()) {
        const int index = pending.back();
        pending.pop_back();
        component.push_back(index);
        const int x = index % width;
        const int y = index / width;
        touchesBoundary = touchesBoundary || x == 0 || y == 0 || x == width - 1 || y == height - 1;
        const int neighbors[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
        for (const auto& neighbor : neighbors) {
          const int neighborX = neighbor[0];
          const int neighborY = neighbor[1];
          if (neighborX < 0 || neighborY < 0 || neighborX >= width || neighborY >= height) {
            continue;
          }
          const int neighborIndex = neighborY * width + neighborX;
          if (visited[static_cast<size_t>(neighborIndex)] ||
              mask.value(neighborX, neighborY) >= threshold) {
            continue;
          }
          visited[static_cast<size_t>(neighborIndex)] = true;
          pending.push_back(neighborIndex);
        }
      }
      if (touchesBoundary || (maximumArea > 0 && static_cast<int>(component.size()) > maximumArea)) {
        continue;
      }
      for (const int index : component) {
        mask.setValue(index % width, index / width, 1.0f);
      }
    }
  }
  return true;
}

// Removes isolated foreground islands up to maximumArea. A zero limit is a
// no-op, which keeps the operation safe for UI defaults.
inline bool removeSegmentationSmallComponents(
    DepthMap& mask, const SegmentationSmallComponentOptions& options = {}) {
  if (mask.isEmpty() || options.maximumArea <= 0) { return false; }
  const int width = mask.width();
  const int height = mask.height();
  const float threshold = std::clamp(options.foregroundThreshold, 0.0f, 1.0f);
  std::vector<bool> visited(static_cast<size_t>(width) * height, false);
  std::vector<int> pending;
  std::vector<int> component;
  pending.reserve(static_cast<size_t>(width) * height / 8 + 1);
  component.reserve(static_cast<size_t>(width) * height / 8 + 1);
  for (int startY = 0; startY < height; ++startY) {
    for (int startX = 0; startX < width; ++startX) {
      const int start = startY * width + startX;
      if (visited[static_cast<size_t>(start)] || mask.value(startX, startY) < threshold) {
        continue;
      }
      pending.clear();
      component.clear();
      pending.push_back(start);
      visited[static_cast<size_t>(start)] = true;
      while (!pending.empty()) {
        const int index = pending.back();
        pending.pop_back();
        component.push_back(index);
        const int x = index % width;
        const int y = index / width;
        const int neighbors[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
        for (const auto& neighbor : neighbors) {
          const int neighborX = neighbor[0];
          const int neighborY = neighbor[1];
          if (neighborX < 0 || neighborY < 0 || neighborX >= width || neighborY >= height) {
            continue;
          }
          const int neighborIndex = neighborY * width + neighborX;
          if (visited[static_cast<size_t>(neighborIndex)] ||
              mask.value(neighborX, neighborY) < threshold) {
            continue;
          }
          visited[static_cast<size_t>(neighborIndex)] = true;
          pending.push_back(neighborIndex);
        }
      }
      if (static_cast<int>(component.size()) > options.maximumArea) { continue; }
      for (const int index : component) {
        mask.setValue(index % width, index / width, 0.0f);
      }
    }
  }
  return true;
}

inline bool refineSegmentationMask(
    DepthMap& mask, const SegmentationMaskRefinementOptions& options) noexcept {
  if (mask.isEmpty()) { return false; }

  const float threshold = std::clamp(options.threshold, 0.0f, 1.0f);
  const float softness = std::clamp(options.softness, 0.0f, 1.0f);
  for (int y = 0; y < mask.height(); ++y) {
    for (int x = 0; x < mask.width(); ++x) {
      const float value = mask.value(x, y);
      const float low = std::max(0.0f, threshold - softness * 0.5f);
      const float high = std::min(1.0f, threshold + softness * 0.5f);
      const float refined = threshold <= 0.0f ? value :
          (high > low
              ? std::clamp((value - low) / (high - low), 0.0f, 1.0f)
              : (value >= threshold ? 1.0f : 0.0f));
      mask.setValue(x, y, refined);
    }
  }

  const int morphologyRadius = std::min(std::abs(options.expandPixels), 64);
  if (morphologyRadius > 0) {
    const DepthMap source = mask;
    DepthMap horizontal;
    horizontal.resize(mask.width(), mask.height());
    const bool growForeground = options.expandPixels > 0;
    for (int y = 0; y < mask.height(); ++y) {
      for (int x = 0; x < mask.width(); ++x) {
        float value = growForeground ? 0.0f : 1.0f;
        for (int dx = -morphologyRadius; dx <= morphologyRadius; ++dx) {
          const float sample = source.value(
              std::clamp(x + dx, 0, mask.width() - 1), y);
          value = growForeground ? std::max(value, sample) : std::min(value, sample);
        }
        horizontal.setValue(x, y, value);
      }
    }
    for (int y = 0; y < mask.height(); ++y) {
      for (int x = 0; x < mask.width(); ++x) {
        float value = growForeground ? 0.0f : 1.0f;
        for (int dy = -morphologyRadius; dy <= morphologyRadius; ++dy) {
          const float sample = horizontal.value(
              x, std::clamp(y + dy, 0, mask.height() - 1));
          value = growForeground ? std::max(value, sample) : std::min(value, sample);
        }
        mask.setValue(x, y, value);
      }
    }
  }

  if (options.fillHoles) {
    SegmentationHoleFillOptions holeFillOptions;
    holeFillOptions.backgroundThreshold = threshold > 0.0f ? threshold : 0.5f;
    holeFillOptions.maximumArea = options.maximumHoleArea;
    fillSegmentationMaskHoles(mask, holeFillOptions);
  }
  if (options.maximumSmallComponentArea > 0) {
    SegmentationSmallComponentOptions componentOptions;
    componentOptions.foregroundThreshold = threshold > 0.0f ? threshold : 0.5f;
    componentOptions.maximumArea = options.maximumSmallComponentArea;
    removeSegmentationSmallComponents(mask, componentOptions);
  }

  const int featherRadius = std::min(std::max(options.featherPixels, 0), 64);
  if (featherRadius > 0) {
    const DepthMap source = mask;
    DepthMap horizontal;
    horizontal.resize(mask.width(), mask.height());
    const int diameter = featherRadius * 2 + 1;
    const float inverseDiameter = 1.0f / static_cast<float>(diameter);
    for (int y = 0; y < mask.height(); ++y) {
      for (int x = 0; x < mask.width(); ++x) {
        float sum = 0.0f;
        for (int dx = -featherRadius; dx <= featherRadius; ++dx) {
          sum += source.value(std::clamp(x + dx, 0, mask.width() - 1), y);
        }
        horizontal.setValue(x, y, sum * inverseDiameter);
      }
    }
    for (int y = 0; y < mask.height(); ++y) {
      for (int x = 0; x < mask.width(); ++x) {
        float sum = 0.0f;
        for (int dy = -featherRadius; dy <= featherRadius; ++dy) {
          sum += horizontal.value(x, std::clamp(y + dy, 0, mask.height() - 1));
        }
        mask.setValue(x, y, sum * inverseDiameter);
      }
    }
  }
  return true;
}

// The caller owns all images and output masks. This intentionally performs no
// image mutation, so a UI can stage results for preview, undo, or export.
struct SegmentationBatchItem {
  const ImageF32x4_RGBA* source = nullptr;
  DepthMap* outputMask = nullptr;
};

struct SegmentationBatchOptions {
  SegmentationMaskRefinementOptions refinement;
  bool refine = true;
};

struct SegmentationBatchResult {
  int completed = 0;
  int failed = 0;
  QString lastError;
};

struct SegmentationMaskStatistics {
  int foregroundPixels = 0;
  float foregroundCoverage = 0.0f;
  float meanConfidence = 0.0f;
  int left = 0;
  int top = 0;
  int right = -1;
  int bottom = -1;

  bool hasForeground() const noexcept { return foregroundPixels > 0; }
};

struct SegmentationMaskDifferenceStatistics {
  float meanAbsoluteDifference = 0.0f;
  float maximumAbsoluteDifference = 0.0f;
  float changedCoverage = 0.0f;
};

struct SegmentationForegroundBounds {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  bool isValid() const noexcept { return width > 0 && height > 0; }
};

struct SegmentationMaskAcceptanceOptions {
  float foregroundThreshold = 0.5f;
  float minimumCoverage = 0.0001f;
  float maximumCoverage = 0.9999f;
  float minimumMeanConfidence = 0.0f;
};

enum class SegmentationMaskCombineMode {
  Replace,
  Union,
  Intersect,
  Subtract,
};

// Resamples sourceMask into targetMask's coordinates. This keeps masks from
// different model resolutions composable without converting through QImage.
inline bool combineSegmentationMasks(
    DepthMap& targetMask, const DepthMap& sourceMask,
    SegmentationMaskCombineMode mode) noexcept {
  if (targetMask.isEmpty() || sourceMask.isEmpty()) { return false; }
  for (int y = 0; y < targetMask.height(); ++y) {
    const float normalizedY = targetMask.height() > 1
        ? static_cast<float>(y) / static_cast<float>(targetMask.height() - 1) : 0.0f;
    for (int x = 0; x < targetMask.width(); ++x) {
      const float normalizedX = targetMask.width() > 1
          ? static_cast<float>(x) / static_cast<float>(targetMask.width() - 1) : 0.0f;
      const float target = targetMask.value(x, y);
      const float source = sourceMask.sampleBilinear(normalizedX, normalizedY);
      float combined = source;
      switch (mode) {
      case SegmentationMaskCombineMode::Union:
        combined = std::max(target, source);
        break;
      case SegmentationMaskCombineMode::Intersect:
        combined = std::min(target, source);
        break;
      case SegmentationMaskCombineMode::Subtract:
        combined = std::max(0.0f, target - source);
        break;
      case SegmentationMaskCombineMode::Replace:
        break;
      }
      targetMask.setValue(x, y, combined);
    }
  }
  return true;
}

// Generates a neutral straight-alpha preview for inspection without changing
// the source image or converting the mask through QImage.
inline bool renderSegmentationMaskPreview(
    const DepthMap& mask, ImageF32x4_RGBA& preview, bool invert = false) {
  if (mask.isEmpty()) { return false; }
  preview.resize(mask.width(), mask.height());
  for (int y = 0; y < mask.height(); ++y) {
    for (int x = 0; x < mask.width(); ++x) {
      float value = mask.value(x, y);
      if (invert) { value = 1.0f - value; }
      preview.setPixel(x, y, FloatRGBA(value, value, value, 1.0f));
    }
  }
  return true;
}

inline SegmentationMaskStatistics analyzeSegmentationMask(
    const DepthMap& mask, float foregroundThreshold = 0.5f) noexcept {
  SegmentationMaskStatistics statistics;
  if (mask.isEmpty()) { return statistics; }
  const float threshold = std::clamp(foregroundThreshold, 0.0f, 1.0f);
  float confidenceSum = 0.0f;
  for (int y = 0; y < mask.height(); ++y) {
    for (int x = 0; x < mask.width(); ++x) {
      const float value = mask.value(x, y);
      confidenceSum += value;
      if (value < threshold) { continue; }
      if (statistics.foregroundPixels == 0) {
        statistics.left = statistics.right = x;
        statistics.top = statistics.bottom = y;
      } else {
        statistics.left = std::min(statistics.left, x);
        statistics.top = std::min(statistics.top, y);
        statistics.right = std::max(statistics.right, x);
        statistics.bottom = std::max(statistics.bottom, y);
      }
      ++statistics.foregroundPixels;
    }
  }
  const float totalPixels = static_cast<float>(mask.width()) * mask.height();
  statistics.foregroundCoverage = static_cast<float>(statistics.foregroundPixels) /
      totalPixels;
  statistics.meanConfidence = confidenceSum / totalPixels;
  return statistics;
}

// Measures frame-to-frame matte change in normalized coordinates. It does not
// motion-compensate; use Roto Brush propagation before this diagnostic when
// the subject moves between frames.
inline SegmentationMaskDifferenceStatistics analyzeSegmentationMaskDifference(
    const DepthMap& previousMask, const DepthMap& currentMask,
    float changedThreshold = 0.1f) noexcept {
  SegmentationMaskDifferenceStatistics statistics;
  if (previousMask.isEmpty() || currentMask.isEmpty()) { return statistics; }
  const float threshold = std::clamp(changedThreshold, 0.0f, 1.0f);
  int changedPixels = 0;
  const int pixelCount = currentMask.width() * currentMask.height();
  for (int y = 0; y < currentMask.height(); ++y) {
    const float normalizedY = currentMask.height() > 1
        ? static_cast<float>(y) / static_cast<float>(currentMask.height() - 1) : 0.0f;
    for (int x = 0; x < currentMask.width(); ++x) {
      const float normalizedX = currentMask.width() > 1
          ? static_cast<float>(x) / static_cast<float>(currentMask.width() - 1) : 0.0f;
      const float difference = std::abs(
          currentMask.value(x, y) - previousMask.sampleBilinear(normalizedX, normalizedY));
      statistics.meanAbsoluteDifference += difference;
      statistics.maximumAbsoluteDifference = std::max(statistics.maximumAbsoluteDifference, difference);
      if (difference >= threshold) { ++changedPixels; }
    }
  }
  statistics.meanAbsoluteDifference /= static_cast<float>(pixelCount);
  statistics.changedCoverage = static_cast<float>(changedPixels) / static_cast<float>(pixelCount);
  return statistics;
}

// Returns a source-image rectangle around the accepted foreground. Padding is
// clamped at the image edge, so it is safe to pass straight to crop().
inline SegmentationForegroundBounds findSegmentationForegroundBounds(
    const DepthMap& mask, float foregroundThreshold = 0.5f,
    int paddingPixels = 0) noexcept {
  SegmentationForegroundBounds bounds;
  const auto statistics = analyzeSegmentationMask(mask, foregroundThreshold);
  if (!statistics.hasForeground()) { return bounds; }
  const int padding = std::max(paddingPixels, 0);
  const int left = std::max(statistics.left - padding, 0);
  const int top = std::max(statistics.top - padding, 0);
  const int right = std::min(statistics.right + padding, mask.width() - 1);
  const int bottom = std::min(statistics.bottom + padding, mask.height() - 1);
  bounds.x = left;
  bounds.y = top;
  bounds.width = right - left + 1;
  bounds.height = bottom - top + 1;
  return bounds;
}

inline bool acceptsSegmentationMask(
    const DepthMap& mask, const SegmentationMaskAcceptanceOptions& options = {}) noexcept {
  if (mask.isEmpty()) { return false; }
  const auto statistics = analyzeSegmentationMask(mask, options.foregroundThreshold);
  const float minimumCoverage = std::clamp(options.minimumCoverage, 0.0f, 1.0f);
  const float maximumCoverage = std::clamp(
      std::max(options.maximumCoverage, minimumCoverage), 0.0f, 1.0f);
  return statistics.hasForeground() &&
      statistics.foregroundCoverage >= minimumCoverage &&
      statistics.foregroundCoverage <= maximumCoverage &&
      statistics.meanConfidence >= std::clamp(options.minimumMeanConfidence, 0.0f, 1.0f);
}

// Blends a previous frame's mask into the current result. This is not optical
// flow: callers should use it only for already aligned frames or as a modest
// anti-flicker pass after model inference.
inline bool stabilizeSegmentationMask(
    const DepthMap& previousMask, DepthMap& currentMask, float temporalStrength) noexcept {
  if (previousMask.isEmpty() || currentMask.isEmpty()) { return false; }
  const float strength = std::clamp(temporalStrength, 0.0f, 1.0f);
  if (strength <= 0.0f) { return true; }
  for (int y = 0; y < currentMask.height(); ++y) {
    const float normalizedY = currentMask.height() > 1
        ? static_cast<float>(y) / static_cast<float>(currentMask.height() - 1) : 0.0f;
    for (int x = 0; x < currentMask.width(); ++x) {
      const float normalizedX = currentMask.width() > 1
          ? static_cast<float>(x) / static_cast<float>(currentMask.width() - 1) : 0.0f;
      const float current = currentMask.value(x, y);
      const float previous = previousMask.sampleBilinear(normalizedX, normalizedY);
      currentMask.setValue(x, y, current + (previous - current) * strength);
    }
  }
  return true;
}

inline SegmentationBatchResult segmentBatch(
    IImageSegmenter& segmenter,
    std::span<const SegmentationBatchItem> items,
    const SegmentationBatchOptions& options = {}) {
  SegmentationBatchResult result;
  if (!segmenter.isReady()) {
    result.failed = static_cast<int>(items.size());
    result.lastError = segmenter.lastError();
    return result;
  }
  for (const auto& item : items) {
    if (!item.source || !item.outputMask ||
        !segmenter.segment(*item.source, *item.outputMask) ||
        (options.refine && !refineSegmentationMask(*item.outputMask, options.refinement))) {
      ++result.failed;
      result.lastError = item.source && item.outputMask
          ? segmenter.lastError()
          : QStringLiteral("Segmentation batch item has no source or output mask.");
      continue;
    }
    ++result.completed;
  }
  return result;
}

inline bool applySegmentationMask(ImageF32x4_RGBA& image,
                                  const DepthMap& mask,
                                  const SegmentationMaskOptions& options) noexcept {
  if (image.isEmpty() || mask.isEmpty()) { return false; }

  const float opacity = std::clamp(options.opacity, 0.0f, 1.0f);
  const float threshold = std::clamp(options.threshold, 0.0f, 1.0f);
  const float softness = std::clamp(options.softness, 0.0f, 1.0f);
  const int width = image.width();
  const int height = image.height();

  for (int y = 0; y < height; ++y) {
    const float normalizedY = height > 1
        ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
    for (int x = 0; x < width; ++x) {
      const float normalizedX = width > 1
          ? static_cast<float>(x) / static_cast<float>(width - 1) : 0.0f;
      float foreground = mask.sampleBilinear(normalizedX, normalizedY);
      if (options.invert) { foreground = 1.0f - foreground; }
      if (threshold > 0.0f) {
        const float low = std::max(0.0f, threshold - softness * 0.5f);
        const float high = std::min(1.0f, threshold + softness * 0.5f);
        foreground = high > low
            ? std::clamp((foreground - low) / (high - low), 0.0f, 1.0f)
            : (foreground >= threshold ? 1.0f : 0.0f);
      }

      auto pixel = image.getPixel(x, y);
      const float targetAlpha = options.mode == SegmentationMaskMode::ReplaceAlpha
          ? foreground : pixel.a() * foreground;
      pixel.setAlpha(pixel.a() + (targetAlpha - pixel.a()) * opacity);
      image.setPixel(x, y, pixel);
    }
  }
  return true;
}

inline void applySegmentationMask(ImageF32x4_RGBA& image,
                                  const DepthMap& mask,
                                  float opacity) noexcept {
  SegmentationMaskOptions options;
  options.opacity = opacity;
  applySegmentationMask(image, mask, options);
}

// Suppresses a green or blue screen color only where the segmentation mask is
// partially covered. It operates on straight RGBA pixels and leaves alpha
// untouched, so it can run before or after alpha application.
inline bool despillSegmentationEdges(
    ImageF32x4_RGBA& image, const DepthMap& mask,
    const SegmentationDespillOptions& options = {}) noexcept {
  if (image.isEmpty() || mask.isEmpty()) { return false; }
  const float strength = std::clamp(options.strength, 0.0f, 1.0f);
  const float edgeWidth = std::max(options.edgeWidth, 0.0001f);
  if (strength <= 0.0f) { return true; }
  for (int y = 0; y < image.height(); ++y) {
    const float normalizedY = image.height() > 1
        ? static_cast<float>(y) / static_cast<float>(image.height() - 1) : 0.0f;
    for (int x = 0; x < image.width(); ++x) {
      const float normalizedX = image.width() > 1
          ? static_cast<float>(x) / static_cast<float>(image.width() - 1) : 0.0f;
      const float coverage = mask.sampleBilinear(normalizedX, normalizedY);
      const float edgeFactor = std::clamp(
          std::min(coverage, 1.0f - coverage) / edgeWidth, 0.0f, 1.0f);
      if (edgeFactor <= 0.0f) { continue; }
      auto pixel = image.getPixel(x, y);
      const float red = pixel.r();
      const float green = pixel.g();
      const float blue = pixel.b();
      const float neutral = options.channel == SegmentationSpillChannel::Green
          ? std::max(red, blue) : std::max(red, green);
      const float chroma = options.channel == SegmentationSpillChannel::Green
          ? green : blue;
      const float correction = std::max(chroma - neutral, 0.0f) * strength * edgeFactor;
      if (options.channel == SegmentationSpillChannel::Green) {
        pixel.setGreen(green - correction);
      } else {
        pixel.setBlue(blue - correction);
      }
      image.setPixel(x, y, pixel);
    }
  }
  return true;
}

// Produces a straight-alpha foreground cutout without mutating the source.
// It shares the exact thresholding semantics used by applySegmentationMask.
inline bool extractSegmentationForeground(
    const ImageF32x4_RGBA& source, const DepthMap& mask,
    ImageF32x4_RGBA& foreground,
    const SegmentationMaskOptions& options = {}) {
  if (source.isEmpty() || mask.isEmpty()) { return false; }
  foreground = source.DeepCopy();
  return applySegmentationMask(foreground, mask, options);
}

// Extract first so mask sampling remains in the original coordinate system,
// then crop. This prevents a low-resolution model mask from shifting when a
// tight foreground crop is requested.
inline bool cropSegmentationForeground(
    const ImageF32x4_RGBA& source, const DepthMap& mask,
    ImageF32x4_RGBA& foreground,
    const SegmentationMaskOptions& options = {},
    float foregroundThreshold = 0.5f, int paddingPixels = 0) {
  const auto bounds = findSegmentationForegroundBounds(
      mask, foregroundThreshold, paddingPixels);
  if (source.isEmpty() || !bounds.isValid() ||
      !extractSegmentationForeground(source, mask, foreground, options)) {
    return false;
  }
  const float left = mask.width() > 1
      ? static_cast<float>(bounds.x) / static_cast<float>(mask.width() - 1) : 0.0f;
  const float top = mask.height() > 1
      ? static_cast<float>(bounds.y) / static_cast<float>(mask.height() - 1) : 0.0f;
  const float right = mask.width() > 1
      ? static_cast<float>(bounds.x + bounds.width - 1) / static_cast<float>(mask.width() - 1) : 1.0f;
  const float bottom = mask.height() > 1
      ? static_cast<float>(bounds.y + bounds.height - 1) / static_cast<float>(mask.height() - 1) : 1.0f;
  const int cropLeft = std::clamp(
      static_cast<int>(std::floor(left * static_cast<float>(source.width() - 1))),
      0, source.width() - 1);
  const int cropTop = std::clamp(
      static_cast<int>(std::floor(top * static_cast<float>(source.height() - 1))),
      0, source.height() - 1);
  const int cropRight = std::clamp(
      static_cast<int>(std::ceil(right * static_cast<float>(source.width() - 1))),
      cropLeft, source.width() - 1);
  const int cropBottom = std::clamp(
      static_cast<int>(std::ceil(bottom * static_cast<float>(source.height() - 1))),
      cropTop, source.height() - 1);
  foreground = foreground.crop(
      cropLeft, cropTop, cropRight - cropLeft + 1, cropBottom - cropTop + 1);
  return true;
}

inline FloatRGBA compositeSegmentationPixel(
    const FloatRGBA& foreground, float foregroundMask,
    const FloatRGBA& background) noexcept {
  const float foregroundAlpha = std::clamp(foreground.a() * foregroundMask, 0.0f, 1.0f);
  const float backgroundAlpha = std::clamp(background.a(), 0.0f, 1.0f);
  const float outputAlpha = foregroundAlpha + backgroundAlpha * (1.0f - foregroundAlpha);
  if (outputAlpha <= 0.0f) { return {}; }
  const float foregroundWeight = foregroundAlpha / outputAlpha;
  const float backgroundWeight = backgroundAlpha * (1.0f - foregroundAlpha) / outputAlpha;
  return FloatRGBA(
      foreground.r() * foregroundWeight + background.r() * backgroundWeight,
      foreground.g() * foregroundWeight + background.g() * backgroundWeight,
      foreground.b() * foregroundWeight + background.b() * backgroundWeight,
      outputAlpha);
}

inline FloatRGBA sampleSegmentationBackground(
    const ImageF32x4_RGBA& image, float normalizedX, float normalizedY) noexcept {
  if (image.isEmpty()) { return {}; }
  const float x = std::clamp(normalizedX, 0.0f, 1.0f) * static_cast<float>(image.width() - 1);
  const float y = std::clamp(normalizedY, 0.0f, 1.0f) * static_cast<float>(image.height() - 1);
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, image.width() - 1);
  const int y1 = std::min(y0 + 1, image.height() - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  return FloatRGBA::lerp(
      FloatRGBA::lerp(image.getPixel(x0, y0), image.getPixel(x1, y0), tx),
      FloatRGBA::lerp(image.getPixel(x0, y1), image.getPixel(x1, y1), tx), ty);
}

// Replaces a background with a solid color. The result is straight-alpha and
// does not rely on Qt composition or implicit image format conversion.
inline bool compositeSegmentationOverColor(
    const ImageF32x4_RGBA& source, const DepthMap& mask,
    const FloatRGBA& background, ImageF32x4_RGBA& destination,
    bool invertMask = false) {
  if (source.isEmpty() || mask.isEmpty()) { return false; }
  destination = source.DeepCopy();
  for (int y = 0; y < destination.height(); ++y) {
    const float normalizedY = destination.height() > 1
        ? static_cast<float>(y) / static_cast<float>(destination.height() - 1) : 0.0f;
    for (int x = 0; x < destination.width(); ++x) {
      const float normalizedX = destination.width() > 1
          ? static_cast<float>(x) / static_cast<float>(destination.width() - 1) : 0.0f;
      float foregroundMask = mask.sampleBilinear(normalizedX, normalizedY);
      if (invertMask) { foregroundMask = 1.0f - foregroundMask; }
      destination.setPixel(x, y, compositeSegmentationPixel(
          source.getPixel(x, y), foregroundMask, background));
    }
  }
  return true;
}

// Background images are bilinearly sampled in normalized coordinates, so
// callers can combine arbitrary source and replacement resolutions without
// QImage.
inline bool compositeSegmentationOverImage(
    const ImageF32x4_RGBA& source, const DepthMap& mask,
    const ImageF32x4_RGBA& background, ImageF32x4_RGBA& destination,
    bool invertMask = false) {
  if (source.isEmpty() || mask.isEmpty() || background.isEmpty()) { return false; }
  destination = source.DeepCopy();
  for (int y = 0; y < destination.height(); ++y) {
    const float normalizedY = destination.height() > 1
        ? static_cast<float>(y) / static_cast<float>(destination.height() - 1) : 0.0f;
    for (int x = 0; x < destination.width(); ++x) {
      const float normalizedX = destination.width() > 1
          ? static_cast<float>(x) / static_cast<float>(destination.width() - 1) : 0.0f;
      float foregroundMask = mask.sampleBilinear(normalizedX, normalizedY);
      if (invertMask) { foregroundMask = 1.0f - foregroundMask; }
      destination.setPixel(x, y, compositeSegmentationPixel(
          source.getPixel(x, y), foregroundMask,
          sampleSegmentationBackground(background, normalizedX, normalizedY)));
    }
  }
  return true;
}
}
