module;
#include <algorithm>
#include <cmath>
#include <limits>

module Image.DeepImageBuffer;

namespace ArtifactCore {
namespace {
bool finiteSample(const DeepSample& sample) {
    if (!std::isfinite(sample.depth) || !std::isfinite(sample.depthBack) ||
        !std::isfinite(sample.alpha) ||
        !std::isfinite(sample.coverage)) return false;
    for (const float channel : sample.color) {
        if (!std::isfinite(channel)) return false;
    }
    return true;
}
}

DeepImageBuffer::DeepImageBuffer(int width, int height) {
    resize(width, height);
}

bool DeepImageBuffer::resize(int width, int height) {
    if (width <= 0 || height <= 0 ||
        static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() /
                                          static_cast<std::size_t>(height)) {
        width_ = height_ = 0;
        pixels_.clear();
        return false;
    }
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<std::size_t>(width) * height, DeepPixel{});
    return true;
}

DeepPixel* DeepImageBuffer::pixel(int x, int y) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return nullptr;
    return &pixels_[static_cast<std::size_t>(y) * width_ + x];
}

const DeepPixel* DeepImageBuffer::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return nullptr;
    return &pixels_[static_cast<std::size_t>(y) * width_ + x];
}

std::size_t DeepImageBuffer::totalSampleCount() const {
    std::size_t count = 0;
    for (const auto& pixelValue : pixels_) {
        if (pixelValue.samples.size() > std::numeric_limits<std::size_t>::max() - count)
            return std::numeric_limits<std::size_t>::max();
        count += pixelValue.samples.size();
    }
    return count;
}

std::size_t DeepImageBuffer::approximateMemoryBytes() const {
    if (pixels_.capacity() > std::numeric_limits<std::size_t>::max() / sizeof(DeepPixel))
        return std::numeric_limits<std::size_t>::max();
    std::size_t bytes = pixels_.capacity() * sizeof(DeepPixel);
    for (const auto& pixelValue : pixels_) {
        if (pixelValue.samples.capacity() >
            (std::numeric_limits<std::size_t>::max() - bytes) / sizeof(DeepSample))
            return std::numeric_limits<std::size_t>::max();
        bytes += pixelValue.samples.capacity() * sizeof(DeepSample);
    }
    return bytes;
}

bool DeepImageBuffer::addSample(int x, int y, const DeepSample& sample) {
    if (!finiteSample(sample)) return false;
    auto* target = pixel(x, y);
    if (!target) return false;
    DeepSample normalized = sample;
    if (normalized.depthBack < normalized.depth) normalized.depthBack = normalized.depth;
    normalized.alpha = std::clamp(normalized.alpha, 0.0f, 1.0f);
    normalized.coverage = std::clamp(normalized.coverage, 0.0f, 1.0f);
    target->samples.push_back(normalized);
    return true;
}

bool DeepImageBuffer::normalizeSamples() {
    if (isEmpty()) return false;
    for (auto& pixelValue : pixels_) {
        for (auto& sample : pixelValue.samples) {
            if (!finiteSample(sample)) return false;
            sample.depthBack = std::max(sample.depth, sample.depthBack);
            sample.alpha = std::clamp(sample.alpha, 0.0f, 1.0f);
            sample.coverage = std::clamp(sample.coverage, 0.0f, 1.0f);
        }
    }
    sortSamplesByDepth();
    return true;
}

void DeepImageBuffer::sortSamplesByDepth() {
    for (auto& pixelValue : pixels_) {
        std::stable_sort(pixelValue.samples.begin(), pixelValue.samples.end(),
                         [](const DeepSample& lhs, const DeepSample& rhs) {
                             return lhs.depth < rhs.depth;
                         });
    }
}

bool DeepImageBuffer::clipDepthRange(float nearDepth, float farDepth) {
    if (!std::isfinite(nearDepth) || !std::isfinite(farDepth) || nearDepth > farDepth)
        return false;
    for (auto& pixelValue : pixels_) {
        pixelValue.samples.erase(
            std::remove_if(pixelValue.samples.begin(), pixelValue.samples.end(),
                           [nearDepth, farDepth](const DeepSample& sample) {
                               if (!finiteSample(sample)) return true;
                               const float back = std::max(sample.depth, sample.depthBack);
                               return back < nearDepth || sample.depth > farDepth;
                           }),
            pixelValue.samples.end());
    }
    return true;
}

void DeepImageBuffer::prune(float minimumAlpha, std::size_t maxSamplesPerPixel) {
    if (!std::isfinite(minimumAlpha)) minimumAlpha = 1.0e-5f;
    minimumAlpha = std::clamp(minimumAlpha, 0.0f, 1.0f);
    if (maxSamplesPerPixel > 0) sortSamplesByDepth();
    for (auto& pixelValue : pixels_) {
        pixelValue.samples.erase(
            std::remove_if(pixelValue.samples.begin(), pixelValue.samples.end(),
                           [minimumAlpha](const DeepSample& sample) {
                               return !finiteSample(sample) ||
                                      sample.alpha * sample.coverage <= minimumAlpha;
                           }),
            pixelValue.samples.end());
        if (maxSamplesPerPixel > 0 && pixelValue.samples.size() > maxSamplesPerPixel) {
            pixelValue.samples.resize(maxSamplesPerPixel);
        }
    }
}

DeepImageBuffer DeepImageBuffer::fromFlatRGBA(const float* rgba, int width, int height) {
    DeepImageBuffer result;
    if (!rgba || !result.resize(width, height)) return result;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
            DeepSample sample;
            for (int channel = 0; channel < 4; ++channel) sample.color[channel] = rgba[offset + channel];
            sample.alpha = std::clamp(sample.color[3], 0.0f, 1.0f);
            sample.color[3] = sample.alpha;
            if (!result.addSample(x, y, sample)) return DeepImageBuffer{};
        }
    }
    return result;
}

DeepImageBuffer DeepImageBuffer::fromFlatRGBAWithDepth(const float* rgba,
                                                       const float* depth,
                                                       int width, int height) {
    DeepImageBuffer result;
    if (!rgba || !depth || !result.resize(width, height)) return result;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y) * width + x;
            DeepSample sample;
            for (int channel = 0; channel < 4; ++channel)
                sample.color[channel] = rgba[pixel * 4u + static_cast<std::size_t>(channel)];
            sample.alpha = std::clamp(sample.color[3], 0.0f, 1.0f);
            sample.color[3] = sample.alpha;
            sample.depth = depth[pixel];
            sample.depthBack = sample.depth;
            if (!result.addSample(x, y, sample)) return DeepImageBuffer{};
        }
    }
    return result;
}

bool DeepImageBuffer::addFlatRGBAAtDepth(const float* rgba, int width, int height,
                                         float depth) {
    if (!rgba || width != width_ || height != height_ || !std::isfinite(depth) ||
        isEmpty()) return false;
    bool success = true;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y) * width + x;
            DeepSample sample;
            for (int channel = 0; channel < 4; ++channel)
                sample.color[channel] = rgba[pixel * 4u + static_cast<std::size_t>(channel)];
            sample.alpha = std::clamp(sample.color[3], 0.0f, 1.0f);
            sample.color[3] = sample.alpha;
            sample.depth = depth;
            sample.depthBack = depth;
            success = addSample(x, y, sample) && success;
        }
    }
    return success;
}

bool DeepImageBuffer::toFlatRGBA(float* rgba, std::size_t floatCount) const {
    if (!rgba || isEmpty() || floatCount < static_cast<std::size_t>(width_) * height_ * 4) return false;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto* source = pixel(x, y);
            const std::size_t offset = (static_cast<std::size_t>(y) * width_ + x) * 4;
            std::array<float, 4> accumulated{0.0f, 0.0f, 0.0f, 0.0f};
            float transmittance = 1.0f;
            if (source) {
                for (const auto& sample : source->samples) {
                    if (!finiteSample(sample)) continue;
                    const float alpha = std::clamp(sample.alpha * sample.coverage, 0.0f, 1.0f);
                    if (!sample.holdout) {
                        for (int channel = 0; channel < 3; ++channel) {
                            accumulated[channel] += sample.color[channel] * alpha * transmittance;
                        }
                    }
                    transmittance *= 1.0f - alpha;
                }
            }
            accumulated[3] = 1.0f - transmittance;
            for (int channel = 0; channel < 4; ++channel) rgba[offset + channel] = accumulated[channel];
        }
    }
    return true;
}

bool DeepImageBuffer::toDepthMatteRGBA(float* rgba, std::size_t floatCount,
                                       float nearDepth, float farDepth) const {
    if (!rgba || isEmpty() || !std::isfinite(nearDepth) || !std::isfinite(farDepth) ||
        nearDepth > farDepth ||
        floatCount < static_cast<std::size_t>(width_) * height_ * 4) return false;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto* source = pixel(x, y);
            const std::size_t offset = (static_cast<std::size_t>(y) * width_ + x) * 4;
            float transmittance = 1.0f;
            if (source) {
                for (const auto& sample : source->samples) {
                    const float sampleBack = std::max(sample.depth, sample.depthBack);
                    if (!finiteSample(sample) || sample.holdout ||
                        sampleBack < nearDepth || sample.depth > farDepth) continue;
                    const float alpha = std::clamp(sample.alpha * sample.coverage, 0.0f, 1.0f);
                    transmittance *= 1.0f - alpha;
                }
            }
            const float alpha = 1.0f - transmittance;
            rgba[offset + 0] = 1.0f;
            rgba[offset + 1] = 1.0f;
            rgba[offset + 2] = 1.0f;
            rgba[offset + 3] = alpha;
        }
    }
    return true;
}

bool DeepImageBuffer::toDepthOfFieldRGBA(float* rgba, std::size_t floatCount,
                                         float focalDepth, float focusRange,
                                         int maxBlurRadius) const {
    if (!rgba || isEmpty() || !std::isfinite(focalDepth) ||
        !std::isfinite(focusRange) || focusRange <= 0.0f ||
        maxBlurRadius < 0 || maxBlurRadius > 64 ||
        floatCount < static_cast<std::size_t>(width_) * height_ * 4) {
        return false;
    }

    const auto flattenAt = [this](int x, int y) {
        std::array<float, 4> value{0.0f, 0.0f, 0.0f, 0.0f};
        const auto* source = pixel(x, y);
        float transmittance = 1.0f;
        if (source) {
            for (const auto& sample : source->samples) {
                if (!finiteSample(sample)) continue;
                const float alpha = std::clamp(sample.alpha * sample.coverage,
                                               0.0f, 1.0f);
                if (!sample.holdout) {
                    for (int channel = 0; channel < 3; ++channel)
                        value[channel] += sample.color[channel] * alpha * transmittance;
                }
                transmittance *= 1.0f - alpha;
            }
        }
        value[3] = 1.0f - transmittance;
        return value;
    };

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto* source = pixel(x, y);
            float representativeDepth = focalDepth;
            if (source) {
                for (const auto& sample : source->samples) {
                    if (finiteSample(sample) && !sample.holdout && sample.alpha > 0.0f) {
                        representativeDepth = sample.depth;
                        break;
                    }
                }
            }
            const int radius = std::clamp(static_cast<int>(std::round(
                std::abs(representativeDepth - focalDepth) / focusRange *
                static_cast<float>(maxBlurRadius))), 0, maxBlurRadius);
            std::array<float, 4> accumulated{0.0f, 0.0f, 0.0f, 0.0f};
            float weightSum = 0.0f;
            for (int oy = -radius; oy <= radius; ++oy) {
                for (int ox = -radius; ox <= radius; ++ox) {
                    if (ox * ox + oy * oy > radius * radius) continue;
                    const int sx = std::clamp(x + ox, 0, width_ - 1);
                    const int sy = std::clamp(y + oy, 0, height_ - 1);
                    const float distance2 = static_cast<float>(ox * ox + oy * oy);
                    const float sigma = std::max(0.5f, radius * 0.5f);
                    const float weight = std::exp(-distance2 /
                                                  (2.0f * sigma * sigma));
                    const auto sample = flattenAt(sx, sy);
                    for (int channel = 0; channel < 4; ++channel)
                        accumulated[channel] += sample[channel] * weight;
                    weightSum += weight;
                }
            }
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width_ + x) * 4;
            for (int channel = 0; channel < 4; ++channel)
                rgba[offset + channel] = weightSum > 0.0f
                    ? accumulated[channel] / weightSum : 0.0f;
        }
    }
    return true;
}

bool DeepImageBuffer::toRankedRGBA(float* rgba, std::size_t floatCount,
                                   std::size_t rank) const {
    if (!rgba || isEmpty() ||
        floatCount < static_cast<std::size_t>(width_) * height_ * 4) return false;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width_ + x) * 4;
            std::array<float, 4> output{0.0f, 0.0f, 0.0f, 0.0f};
            const auto* source = pixel(x, y);
            if (source && rank < source->samples.size()) {
                const auto& sample = source->samples[rank];
                if (finiteSample(sample) && !sample.holdout) {
                    const float coverage = std::clamp(sample.coverage, 0.0f, 1.0f);
                    const float alpha = std::clamp(sample.alpha * coverage, 0.0f, 1.0f);
                    for (int channel = 0; channel < 3; ++channel)
                        output[channel] = sample.color[channel] * alpha;
                    output[3] = alpha;
                }
            }
            for (int channel = 0; channel < 4; ++channel) rgba[offset + channel] = output[channel];
        }
    }
    return true;
}

bool mergeDeepOver(const DeepImageBuffer& front,
                   const DeepImageBuffer& back,
                   DeepImageBuffer& result,
                   std::size_t maxSamplesPerPixel) {
    if (front.isEmpty() || back.isEmpty() || front.width() != back.width() ||
        front.height() != back.height() ||
        !result.resize(front.width(), front.height())) {
        return false;
    }
    for (int y = 0; y < front.height(); ++y) {
        for (int x = 0; x < front.width(); ++x) {
            const auto* frontPixel = front.pixel(x, y);
            const auto* backPixel = back.pixel(x, y);
            auto* outputPixel = result.pixel(x, y);
            if (!frontPixel || !backPixel || !outputPixel) return false;
            if (!std::all_of(frontPixel->samples.begin(), frontPixel->samples.end(),
                             finiteSample) ||
                !std::all_of(backPixel->samples.begin(), backPixel->samples.end(),
                             finiteSample)) {
                return false;
            }
            outputPixel->samples.reserve(frontPixel->samples.size() + backPixel->samples.size());
            outputPixel->samples.insert(outputPixel->samples.end(),
                                        frontPixel->samples.begin(), frontPixel->samples.end());
            outputPixel->samples.insert(outputPixel->samples.end(),
                                        backPixel->samples.begin(), backPixel->samples.end());
        }
    }
    result.sortSamplesByDepth();
    if (maxSamplesPerPixel > 0) result.prune(1.0e-5f, maxSamplesPerPixel);
    return true;
}

bool applyDeepHoldout(const DeepImageBuffer& holdout,
                      DeepImageBuffer& target) {
    if (holdout.isEmpty() || target.isEmpty() || holdout.width() != target.width() ||
        holdout.height() != target.height()) return false;
    for (int y = 0; y < target.height(); ++y) {
        for (int x = 0; x < target.width(); ++x) {
            const auto* mattePixel = holdout.pixel(x, y);
            auto* targetPixel = target.pixel(x, y);
            if (!mattePixel || !targetPixel) return false;
            for (auto& sample : targetPixel->samples) {
                if (!finiteSample(sample) || sample.holdout) continue;
                float remaining = 1.0f;
                for (const auto& matte : mattePixel->samples) {
                    if (!finiteSample(matte)) continue;
                    // A matte only occludes samples at or behind its depth;
                    // foreground samples must remain untouched.
                    const float matteBack = std::max(matte.depth, matte.depthBack);
                    const float sampleBack = std::max(sample.depth, sample.depthBack);
                    if (sampleBack < matte.depth || sample.depth > matteBack) continue;
                    const float alpha = std::clamp(matte.alpha * matte.coverage, 0.0f, 1.0f);
                    remaining *= 1.0f - alpha;
                }
                sample.alpha = std::clamp(sample.alpha * remaining, 0.0f, 1.0f);
                sample.coverage = std::clamp(sample.coverage * remaining, 0.0f, 1.0f);
            }
        }
    }
    return true;
}

bool packDeepImage(const DeepImageBuffer& image, DeepImagePacked& packed) {
    packed = {};
    if (image.isEmpty()) return false;
    if (image.width() <= 0 || image.height() <= 0 ||
        static_cast<std::size_t>(image.width()) >
            std::numeric_limits<std::size_t>::max() /
                static_cast<std::size_t>(image.height())) {
        return false;
    }
    const std::size_t pixelCount = static_cast<std::size_t>(image.width()) *
                                   static_cast<std::size_t>(image.height());
    packed.width = image.width();
    packed.height = image.height();
    packed.sampleOffsets.resize(pixelCount);
    packed.sampleCounts.resize(pixelCount);
    packed.gpuSampleCounts.resize(pixelCount);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto* sourcePixel = image.pixel(x, y);
            if (!sourcePixel || sourcePixel->samples.size() >
                std::numeric_limits<std::uint16_t>::max() ||
                packed.flatSamples.size() > std::numeric_limits<std::uint32_t>::max() ||
                sourcePixel->samples.size() >
                    std::numeric_limits<std::uint32_t>::max() - packed.flatSamples.size()) {
                packed = {};
                return false;
            }
            if (!std::all_of(sourcePixel->samples.begin(), sourcePixel->samples.end(),
                             finiteSample)) {
                packed = {};
                return false;
            }
            const std::size_t index = static_cast<std::size_t>(y) * image.width() + x;
            packed.sampleOffsets[index] = static_cast<std::uint32_t>(packed.flatSamples.size());
            packed.sampleCounts[index] = static_cast<std::uint16_t>(sourcePixel->samples.size());
            packed.gpuSampleCounts[index] = static_cast<std::uint32_t>(sourcePixel->samples.size());
            packed.flatSamples.insert(packed.flatSamples.end(),
                                      sourcePixel->samples.begin(),
                                      sourcePixel->samples.end());
            for (const auto& sample : sourcePixel->samples) {
                DeepSampleGpu gpu;
                gpu.depth = sample.depth;
                gpu.depthBack = sample.depthBack;
                gpu.color = sample.color;
                gpu.alpha = sample.alpha;
                gpu.coverage = sample.coverage;
                gpu.holdout = sample.holdout ? 1u : 0u;
                packed.gpuSamples.push_back(gpu);
            }
        }
    }
    packed.totalSamples = packed.flatSamples.size();
    return packed.buffersAreConsistent();
}

bool unpackDeepImage(const DeepImagePacked& packed, DeepImageBuffer& image) {
    if (!packed.buffersAreConsistent() || packed.flatSamples.size() >
        std::numeric_limits<std::uint32_t>::max()) {
        image.resize(0, 0);
        return false;
    }
    if (!image.resize(packed.width, packed.height)) return false;
    for (int y = 0; y < packed.height; ++y) {
        for (int x = 0; x < packed.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * packed.width + x;
            const std::uint64_t offset = packed.sampleOffsets[index];
            const std::uint64_t count = packed.gpuSampleCounts[index];
            if (offset > packed.flatSamples.size() ||
                count > packed.flatSamples.size() - offset) {
                image.resize(0, 0);
                return false;
            }
            for (std::uint64_t i = 0; i < count; ++i) {
                if (!image.addSample(x, y,
                                     packed.flatSamples[static_cast<std::size_t>(offset + i)])) {
                    image.resize(0, 0);
                    return false;
                }
            }
        }
    }
    return image.normalizeSamples();
}

bool compositeFlatOverDeep(const float* rgba, int width, int height, float depth,
                           DeepImageBuffer& target,
                           std::size_t maxSamplesPerPixel) {
    if (!rgba || width <= 0 || height <= 0 || !std::isfinite(depth)) return false;
    DeepImageBuffer flat = DeepImageBuffer::fromFlatRGBA(rgba, width, height);
    if (flat.isEmpty()) return false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto* pixelValue = flat.pixel(x, y);
            if (!pixelValue) return false;
            for (auto& sample : pixelValue->samples) {
                sample.depth = depth;
                sample.depthBack = depth;
            }
        }
    }
    if (target.isEmpty()) {
        target = std::move(flat);
        return true;
    }
    DeepImageBuffer merged;
    if (!mergeDeepOver(flat, target, merged, maxSamplesPerPixel)) return false;
    target = std::move(merged);
    return true;
}

bool compositeDeepOverFlat(const DeepImageBuffer& source, const float* rgba,
                           std::size_t floatCount, float depth,
                           float* output) {
    if (!output || !rgba || source.isEmpty() || !std::isfinite(depth) ||
        floatCount < static_cast<std::size_t>(source.width()) *
                         static_cast<std::size_t>(source.height()) * 4u) ||
        !source.toFlatRGBA(output, floatCount)) {
        return false;
    }
    DeepImageBuffer foreground = DeepImageBuffer::fromFlatRGBA(
        rgba, source.width(), source.height());
    if (foreground.isEmpty()) return false;
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            auto* pixelValue = foreground.pixel(x, y);
            if (!pixelValue) return false;
            for (auto& sample : pixelValue->samples) {
                sample.depth = depth;
                sample.depthBack = depth;
            }
        }
    }
    DeepImageBuffer merged;
    if (!mergeDeepOver(foreground, source, merged)) return false;
    return merged.toFlatRGBA(output, floatCount);
}

} // namespace ArtifactCore
