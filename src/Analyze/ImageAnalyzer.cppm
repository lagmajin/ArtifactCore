module;
#include <cmath>
#include <algorithm>
#include <array>
module Analyze.Histogram;

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




namespace ArtifactCore {

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

    for (int i = 0; i < total; ++i) {
        float val = std::clamp(pixels[i * 4 + channel], 0.0f, 1.0f);

        // Update min/max
        if (val < stats.min) stats.min = val;
        if (val > stats.max) stats.max = val;

        // Bin into histogram
        int bin = static_cast<int>(val * 255.0f);
        bin = std::clamp(bin, 0, 255);
        stats.rawHistogram[bin]++;
    }

    computeStats(stats);
    return stats;
}

ChannelStatistics ImageAnalyzer::analyzeLuminance(const float* pixels, int width, int height) {
    ChannelStatistics stats{};
    const int total = width * height;
    stats.totalPixels = total;
    stats.min = 1.0f;
    stats.max = 0.0f;

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx + 0];
        float g = pixels[idx + 1];
        float b = pixels[idx + 2];
        // Rec.709 luminance
        float lum = std::clamp(0.2126f * r + 0.7152f * g + 0.0722f * b, 0.0f, 1.0f);

        if (lum < stats.min) stats.min = lum;
        if (lum > stats.max) stats.max = lum;

        int bin = std::clamp(static_cast<int>(lum * 255.0f), 0, 255);
        stats.rawHistogram[bin]++;
    }

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
        float lum = 0.2126f * pixels[idx] + 0.7152f * pixels[idx + 1] + 0.0722f * pixels[idx + 2];
        lum = std::max(lum, 0.0001f); // Avoid log(0)
        sumLum += std::log2(lum);
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
    double sumR = 0, sumG = 0, sumB = 0;

    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        sumR += pixels[idx + 0];
        sumG += pixels[idx + 1];
        sumB += pixels[idx + 2];
    }

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
