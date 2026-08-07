module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

module Audio.Rasterizer;

namespace ArtifactCore {
namespace {
void appendRange(const QVector<float>& samples, int first, int last,
                 QVector<float>& mins, QVector<float>& maxs) {
  float minValue = std::numeric_limits<float>::infinity();
  float maxValue = -std::numeric_limits<float>::infinity();
  for (int i = first; i < last; ++i) {
    const float value = samples.at(i);
    if (!std::isfinite(value)) continue;
    minValue = std::min(minValue, value);
    maxValue = std::max(maxValue, value);
  }
  if (!std::isfinite(minValue)) minValue = maxValue = 0.0f;
  mins.push_back(minValue);
  maxs.push_back(maxValue);
}
}

WaveformData AudioRasterizer::rasterize(const QVector<float>& samples,
                                        const int binCount) {
  if (samples.isEmpty() || binCount <= 0) return {};
  const int bins = std::min(binCount, static_cast<int>(samples.size()));
  WaveformData result;
  result.minValues.reserve(bins);
  result.maxValues.reserve(bins);
  for (int bin = 0; bin < bins; ++bin) {
    const int first = (bin * samples.size()) / bins;
    const int last = std::max(first + 1, ((bin + 1) * static_cast<int>(samples.size())) / bins);
    appendRange(samples, first, std::min(last, static_cast<int>(samples.size())),
                result.minValues, result.maxValues);
  }
  return result;
}

WaveformData AudioRasterizer::rasterizeInterleaved(
    const QVector<float>& samples, const int channels, const int binCount) {
  if (samples.isEmpty() || channels <= 0 || binCount <= 0 ||
      samples.size() < channels) return {};
  const int frameCount = samples.size() / channels;
  const int bins = std::min(binCount, frameCount);
  WaveformData result;
  result.minValues.reserve(bins);
  result.maxValues.reserve(bins);
  for (int bin = 0; bin < bins; ++bin) {
    const int firstFrame = (bin * frameCount) / bins;
    const int lastFrame = std::max(firstFrame + 1,
                                   ((bin + 1) * frameCount) / bins);
    float minValue = std::numeric_limits<float>::infinity();
    float maxValue = -std::numeric_limits<float>::infinity();
    for (int frame = firstFrame; frame < std::min(lastFrame, frameCount);
         ++frame) {
      for (int channel = 0; channel < channels; ++channel) {
        const float value = samples.at(frame * channels + channel);
        if (!std::isfinite(value)) continue;
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
      }
    }
    if (!std::isfinite(minValue)) minValue = maxValue = 0.0f;
    result.minValues.push_back(minValue);
    result.maxValues.push_back(maxValue);
  }
  return result;
}

} // namespace ArtifactCore
