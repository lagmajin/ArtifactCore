module;
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <bit>
#include <limits>
#include <opencv2/core.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Analyze.Histogram;

import Image.ImageSurfaceView;

namespace ArtifactCore {

namespace {

float halfBitsToFloat(const std::uint16_t half) noexcept {
    const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16u;
    std::uint32_t mantissa = half & 0x03ffu;
    const int halfExponent = (half >> 10u) & 0x1fu;
    std::uint32_t bits = sign;
    if (halfExponent == 0) {
        if (mantissa != 0) {
            int exponent = -14;
            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1u;
                --exponent;
            }
            mantissa &= 0x03ffu;
            bits |= static_cast<std::uint32_t>(exponent + 127) << 23u;
            bits |= mantissa << 13u;
        }
    } else if (halfExponent == 0x1f) {
        bits |= 0x7f800000u | (mantissa << 13u);
    } else {
        bits |= static_cast<std::uint32_t>(halfExponent + 112) << 23u;
        bits |= mantissa << 13u;
    }
    return std::bit_cast<float>(bits);
}

}

bool ImageAnalyzer::samplePixel(const ImageSurfaceView& image, int x, int y,
                                ImagePixelSample& sample) noexcept {
    if (!image.isValid() || x < 0 || y < 0 || x >= image.width || y >= image.height ||
        (image.descriptor.channelOrder != SurfaceChannelOrder::RGBA &&
         image.descriptor.channelOrder != SurfaceChannelOrder::BGRA)) {
        return false;
    }

    constexpr std::size_t maxSize = (std::numeric_limits<std::size_t>::max)();
    const bool isFloat32 = image.precision == SurfacePrecision::Float32 &&
        image.descriptor.storage == SurfacePixelStorage::RGBA32Float;
    const bool isFloat16 = image.precision == SurfacePrecision::Float16 &&
        image.descriptor.storage == SurfacePixelStorage::RGBA16Float;
    if (!isFloat16 && !isFloat32)
        return false;
    const std::size_t componentSize = isFloat16 ? sizeof(std::uint16_t) : sizeof(float);
    const std::size_t componentAlignment = isFloat16 ? alignof(std::uint16_t) : alignof(float);
    if (static_cast<std::size_t>(image.width) > maxSize / (4u * componentSize) ||
        reinterpret_cast<std::uintptr_t>(image.data) % componentAlignment != 0 ||
        image.rowStride % componentSize != 0)
        return false;
    const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4u * componentSize;
    const std::size_t precedingRows = static_cast<std::size_t>(image.height - 1);
    if (image.rowStride < rowBytes ||
        precedingRows > (maxSize - rowBytes) / image.rowStride)
        return false;
    const std::size_t span = precedingRows * image.rowStride + rowBytes;
    if (span > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()))
        return false;

    const std::size_t rowOffset = static_cast<std::size_t>(y) * image.rowStride;
    const std::size_t pixelOffset = static_cast<std::size_t>(x) * 4u;
    constexpr auto maxAddress = (std::numeric_limits<std::uintptr_t>::max)();
    if (rowOffset > maxAddress - rowBytes ||
        reinterpret_cast<std::uintptr_t>(image.data) > maxAddress - rowOffset - rowBytes)
        return false;
    const std::size_t componentOffset = pixelOffset * componentSize;
    if (isFloat32) {
        const auto* row = reinterpret_cast<const float*>(
            static_cast<const std::uint8_t*>(image.data) + rowOffset);
        const float* pixel = row + pixelOffset;
        if (image.descriptor.channelOrder == SurfaceChannelOrder::RGBA) {
            sample.rgba = {pixel[0], pixel[1], pixel[2], pixel[3]};
        } else {
            sample.rgba = {pixel[2], pixel[1], pixel[0], pixel[3]};
        }
    } else {
        const auto* pixel = reinterpret_cast<const std::uint16_t*>(
            static_cast<const std::uint8_t*>(image.data) + rowOffset + componentOffset);
        const float c0 = halfBitsToFloat(pixel[0]);
        const float c1 = halfBitsToFloat(pixel[1]);
        const float c2 = halfBitsToFloat(pixel[2]);
        const float c3 = halfBitsToFloat(pixel[3]);
        if (image.descriptor.channelOrder == SurfaceChannelOrder::RGBA)
            sample.rgba = {c0, c1, c2, c3};
        else
            sample.rgba = {c2, c1, c0, c3};
    }
    sample.x = x;
    sample.y = y;
    return true;
}

bool ImageAnalyzer::samplePixel(const ImageByteSurfaceView& image, int x, int y,
                                ImagePixelSample& sample) noexcept {
    if (!image.isValid() || x < 0 || y < 0 || x >= image.width || y >= image.height ||
        (image.descriptor.storage != SurfacePixelStorage::RGBA8UNorm &&
         image.descriptor.storage != SurfacePixelStorage::RGBA8UNormSrgb) ||
        (image.descriptor.channelOrder != SurfaceChannelOrder::RGBA &&
         image.descriptor.channelOrder != SurfaceChannelOrder::BGRA))
        return false;

    constexpr std::size_t maxSize = (std::numeric_limits<std::size_t>::max)();
    const std::size_t width = static_cast<std::size_t>(image.width);
    if (width > maxSize / 4u)
        return false;
    const std::size_t rowBytes = width * 4u;
    const std::size_t precedingRows = static_cast<std::size_t>(image.height - 1);
    if (image.rowStride < rowBytes ||
        precedingRows > (maxSize - rowBytes) / image.rowStride)
        return false;
    const std::size_t span = precedingRows * image.rowStride + rowBytes;
    if (span > static_cast<std::size_t>((std::numeric_limits<std::ptrdiff_t>::max)()))
        return false;

    const std::size_t rowOffset = static_cast<std::size_t>(y) * image.rowStride;
    const std::size_t pixelOffset = static_cast<std::size_t>(x) * 4u;
    constexpr auto maxAddress = (std::numeric_limits<std::uintptr_t>::max)();
    if (rowOffset > maxSize - pixelOffset - 4u || rowOffset > maxAddress - rowBytes ||
        reinterpret_cast<std::uintptr_t>(image.data) > maxAddress - rowOffset - rowBytes)
        return false;
    const std::uint8_t* pixel = image.data + rowOffset + pixelOffset;
    constexpr float scale = 1.0f / 255.0f;
    if (image.descriptor.channelOrder == SurfaceChannelOrder::RGBA) {
        sample.rgba = {pixel[0] * scale, pixel[1] * scale,
                       pixel[2] * scale, pixel[3] * scale};
    } else {
        sample.rgba = {pixel[2] * scale, pixel[1] * scale,
                       pixel[0] * scale, pixel[3] * scale};
    }
    sample.x = x;
    sample.y = y;
    return true;
}

template <typename View>
static bool analyzeSpatialFrequencyView(const View& image,
                                        ImageSpectrumChannel channel,
                                        float* magnitudes, std::size_t capacity,
                                        std::size_t& count) {
    count = 0;
    if (!magnitudes || !image.isValid() || image.width <= 0 || image.height <= 0)
        return false;

    constexpr std::size_t maxSize = (std::numeric_limits<std::size_t>::max)();
    const auto width = static_cast<std::size_t>(image.width);
    const auto height = static_cast<std::size_t>(image.height);
    if (width > maxSize / height)
        return false;
    const std::size_t pixelCount = width * height;
    count = pixelCount;
    if (capacity < pixelCount)
        return false;

    switch (channel) {
    case ImageSpectrumChannel::Red:
    case ImageSpectrumChannel::Green:
    case ImageSpectrumChannel::Blue:
    case ImageSpectrumChannel::Alpha:
    case ImageSpectrumChannel::Rec709WeightedRgb:
        break;
    default:
        count = 0;
        return false;
    }

    ImagePixelSample pixel;
    if (!ImageAnalyzer::samplePixel(image, 0, 0, pixel)) {
        count = 0;
        return false;
    }

    cv::Mat luminance(image.height, image.width, CV_32FC1);
    for (int y = 0; y < image.height; ++y) {
        float* row = luminance.ptr<float>(y);
        for (int x = 0; x < image.width; ++x) {
            if (!ImageAnalyzer::samplePixel(image, x, y, pixel))
                return false;
            float value = 0.0f;
            switch (channel) {
            case ImageSpectrumChannel::Red: value = pixel.rgba[0]; break;
            case ImageSpectrumChannel::Green: value = pixel.rgba[1]; break;
            case ImageSpectrumChannel::Blue: value = pixel.rgba[2]; break;
            case ImageSpectrumChannel::Alpha: value = pixel.rgba[3]; break;
            case ImageSpectrumChannel::Rec709WeightedRgb:
                value = 0.2126f * pixel.rgba[0] + 0.7152f * pixel.rgba[1] +
                        0.0722f * pixel.rgba[2];
                break;
            default:
                count = 0;
                return false;
            }
            if (!std::isfinite(value)) {
                count = 0;
                return false;
            }
            row[x] = value;
        }
    }

    cv::Mat complexSpectrum;
    cv::dft(luminance, complexSpectrum, cv::DFT_COMPLEX_OUTPUT);
    const float inversePixelCount = 1.0f / static_cast<float>(pixelCount);
    for (int y = 0; y < image.height; ++y) {
        const cv::Vec2f* row = complexSpectrum.ptr<cv::Vec2f>(y);
        const std::size_t offset = static_cast<std::size_t>(y) * width;
        for (int x = 0; x < image.width; ++x)
            magnitudes[offset + static_cast<std::size_t>(x)] =
                std::hypot(row[x][0], row[x][1]) * inversePixelCount;
    }
    return true;
}

bool ImageAnalyzer::analyzeSpatialFrequency(const ImageSurfaceView& image,
                                           ImageSpectrumChannel channel,
                                           float* magnitudes, std::size_t capacity,
                                           std::size_t& count) {
    return analyzeSpatialFrequencyView(image, channel, magnitudes, capacity, count);
}

bool ImageAnalyzer::analyzeSpatialFrequency(const ImageByteSurfaceView& image,
                                           ImageSpectrumChannel channel,
                                           float* magnitudes, std::size_t capacity,
                                           std::size_t& count) {
    return analyzeSpatialFrequencyView(image, channel, magnitudes, capacity, count);
}

namespace {

struct ChannelAccum {
    int histogram[256]{};
    float min = 1.0f;
    float max = 0.0f;
};

struct RGBAccum {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

}

// ============================================================
// Internal: compute stats from raw histogram
// ============================================================

static void computeStats(ChannelStatistics& stats) {
    // Normalize histogram
    if (stats.totalPixels > 0) {
        float invTotal = 1.0f / static_cast<float>(stats.totalPixels);
        for (int i = 0; i < 256; ++i) {
            stats.histogram[i] = stats.rawHistogram[i] * invTotal;
        }
    }

    // Mean from histogram
    double sum = 0.0;
    for (int i = 0; i < 256; ++i) {
        sum += (static_cast<double>(i) / 255.0) * stats.rawHistogram[i];
    }
    stats.mean = static_cast<float>(sum / std::max(1, stats.totalPixels));

    // Standard deviation
    double sqSum = 0.0;
    for (int i = 0; i < 256; ++i) {
        double v = static_cast<double>(i) / 255.0 - stats.mean;
        sqSum += v * v * stats.rawHistogram[i];
    }
    stats.stddev = static_cast<float>(std::sqrt(sqSum / std::max(1, stats.totalPixels)));

    // Median and percentiles via CDF
    int cumulative = 0;
    int p5Target = static_cast<int>(stats.totalPixels * 0.05f);
    int p50Target = stats.totalPixels / 2;
    int p95Target = static_cast<int>(stats.totalPixels * 0.95f);
    bool found5 = false, found50 = false, found95 = false;

    for (int i = 0; i < 256; ++i) {
        cumulative += stats.rawHistogram[i];
        float val = static_cast<float>(i) / 255.0f;
        if (!found5 && cumulative >= p5Target) {
            stats.percentile5 = val; found5 = true;
        }
        if (!found50 && cumulative >= p50Target) {
            stats.median = val; found50 = true;
        }
        if (!found95 && cumulative >= p95Target) {
            stats.percentile95 = val; found95 = true;
        }
    }
}

template <typename View>
static bool analyzeView(const View& image, ImageStatistics& statistics) noexcept {
    if (!image.isValid() || image.width <= 0 || image.height <= 0)
        return false;
    const auto width = static_cast<std::size_t>(image.width);
    const auto height = static_cast<std::size_t>(image.height);
    constexpr auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (width > maxSize / height || width * height >
            static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return false;

    ImageStatistics result{};
    ChannelStatistics* channels[] = {
        &result.red, &result.green, &result.blue, &result.alpha, &result.luminance};
    const int totalPixels = static_cast<int>(width * height);
    for (ChannelStatistics* channel : channels) {
        channel->min = 1.0f;
        channel->max = 0.0f;
        channel->totalPixels = totalPixels;
    }
    const auto accumulate = [](ChannelStatistics& channel, float value) noexcept {
        value = std::clamp(value, 0.0f, 1.0f);
        channel.min = std::min(channel.min, value);
        channel.max = std::max(channel.max, value);
        const int bin = std::clamp(static_cast<int>(value * 255.0f), 0, 255);
        ++channel.rawHistogram[bin];
    };

    ImagePixelSample pixel;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            if (!ImageAnalyzer::samplePixel(image, x, y, pixel) ||
                !std::isfinite(pixel.rgba[0]) || !std::isfinite(pixel.rgba[1]) ||
                !std::isfinite(pixel.rgba[2]) || !std::isfinite(pixel.rgba[3]))
                return false;
            accumulate(result.red, pixel.rgba[0]);
            accumulate(result.green, pixel.rgba[1]);
            accumulate(result.blue, pixel.rgba[2]);
            accumulate(result.alpha, pixel.rgba[3]);
            const float weightedRgb = 0.2126f * pixel.rgba[0] +
                                      0.7152f * pixel.rgba[1] +
                                      0.0722f * pixel.rgba[2];
            if (!std::isfinite(weightedRgb))
                return false;
            accumulate(result.luminance, weightedRgb);
        }
    }

    for (ChannelStatistics* channel : channels)
        computeStats(*channel);
    statistics = result;
    return true;
}

bool ImageAnalyzer::analyze(const ImageSurfaceView& image,
                            ImageStatistics& statistics) noexcept {
    return analyzeView(image, statistics);
}

bool ImageAnalyzer::analyze(const ImageByteSurfaceView& image,
                            ImageStatistics& statistics) noexcept {
    return analyzeView(image, statistics);
}

// ============================================================
// ImageAnalyzer
// ============================================================

ChannelStatistics ImageAnalyzer::analyzeChannel(const float* pixels, int width, int height,
                                                  int channel) {
    ChannelStatistics stats{};
    const int total = width * height;
    stats.totalPixels = total;
    stats.min = 1.0f;
    stats.max = 0.0f;

    ChannelAccum accumulated{};
    for (int i = 0; i < total; ++i) {
        float val = std::clamp(pixels[i * 4 + channel], 0.0f, 1.0f);
        accumulated.min = std::min(accumulated.min, val);
        accumulated.max = std::max(accumulated.max, val);
        int bin = std::clamp(static_cast<int>(val * 255.0f), 0, 255);
        ++accumulated.histogram[bin];
    }
    stats.min = accumulated.min;
    stats.max = accumulated.max;
    std::copy(accumulated.histogram, accumulated.histogram + 256, stats.rawHistogram);

    computeStats(stats);
    return stats;
}

ChannelStatistics ImageAnalyzer::analyzeLuminance(const float* pixels, int width, int height) {
    ChannelStatistics stats{};
    const int total = width * height;
    stats.totalPixels = total;
    stats.min = 1.0f;
    stats.max = 0.0f;

    ChannelAccum accumulated{};
    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float lum = std::clamp(0.2126f * pixels[idx + 0] +
                               0.7152f * pixels[idx + 1] +
                               0.0722f * pixels[idx + 2], 0.0f, 1.0f);
        accumulated.min = std::min(accumulated.min, lum);
        accumulated.max = std::max(accumulated.max, lum);
        int bin = std::clamp(static_cast<int>(lum * 255.0f), 0, 255);
        ++accumulated.histogram[bin];
    }
    stats.min = accumulated.min;
    stats.max = accumulated.max;
    std::copy(accumulated.histogram, accumulated.histogram + 256, stats.rawHistogram);

    computeStats(stats);
    return stats;
}

ImageStatistics ImageAnalyzer::analyze(const float* pixels, int width, int height) {
    ImageStatistics result{};
    result.red   = analyzeChannel(pixels, width, height, 0);
    result.green = analyzeChannel(pixels, width, height, 1);
    result.blue  = analyzeChannel(pixels, width, height, 2);
    result.alpha = analyzeChannel(pixels, width, height, 3);
    result.luminance = analyzeLuminance(pixels, width, height);
    return result;
}

float ImageAnalyzer::autoExposureEV(const float* pixels, int width, int height) {
    // Calculate average luminance
    const int total = width * height;
    double sumLum = 0.0;
    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float lum = 0.2126f * pixels[idx] + 0.7152f * pixels[idx + 1] +
                    0.0722f * pixels[idx + 2];
        sumLum += std::log2(std::max(lum, 0.0001f));
    }

    float avgLogLum = static_cast<float>(sumLum / total);
    float avgLum = std::pow(2.0f, avgLogLum);

    // Target: 18% grey (middle grey)
    const float targetGrey = 0.18f;

    // EV adjustment = log2(target / current)
    float ev = std::log2(targetGrey / std::max(avgLum, 0.0001f));
    return ev;
}

std::array<float, 3> ImageAnalyzer::autoWhiteBalance(const float* pixels, int width, int height) {
    // Grey World assumption:
    // Average of all pixels should be grey → compute per-channel multipliers
    const int total = width * height;
    RGBAccum sums{};
    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        sums.r += pixels[idx + 0];
        sums.g += pixels[idx + 1];
        sums.b += pixels[idx + 2];
    }
    const double sumR = sums.r;
    const double sumG = sums.g;
    const double sumB = sums.b;

    double avgR = sumR / total;
    double avgG = sumG / total;
    double avgB = sumB / total;

    // Use green as reference (standard in photography)
    double refGrey = avgG;
    if (refGrey < 0.001) refGrey = 0.001;

    return {
        static_cast<float>(refGrey / std::max(avgR, 0.001)),
        1.0f, // Green stays as-is
        static_cast<float>(refGrey / std::max(avgB, 0.001))
    };
}

float ImageAnalyzer::contrastRatio(const float* pixels, int width, int height) {
    auto lumStats = analyzeLuminance(pixels, width, height);
    float darkest = std::max(lumStats.percentile5, 0.0001f);
    float brightest = std::max(lumStats.percentile95, 0.0001f);
    return brightest / darkest;
}

float ImageAnalyzer::dynamicRange(const float* pixels, int width, int height) {
    float ratio = contrastRatio(pixels, width, height);
    return std::log2(std::max(ratio, 1.0f));
}

float ImageAnalyzer::percentile(const ChannelStatistics& stats, float p) {
    p = std::clamp(p, 0.0f, 1.0f);
    int target = static_cast<int>(stats.totalPixels * p);
    int cumulative = 0;
    for (int i = 0; i < 256; ++i) {
        cumulative += stats.rawHistogram[i];
        if (cumulative >= target) {
            return static_cast<float>(i) / 255.0f;
        }
    }
    return 1.0f;
}

} // namespace ArtifactCore
