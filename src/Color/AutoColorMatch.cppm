module;
#include <cmath>
#include <algorithm>
#include <array>
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
module Color.AutoMatch;





namespace ArtifactCore {

// ============================================================
// Internal: RGB <-> Lab conversion (simplified D65 illuminant)
// ============================================================

static constexpr float D65_X = 0.95047f;
static constexpr float D65_Y = 1.00000f;
static constexpr float D65_Z = 1.08883f;

static float gammaToLinear(float c) {
    return (c > 0.04045f) ? std::pow((c + 0.055f) / 1.055f, 2.4f) : c / 12.92f;
}

static float linearToGamma(float c) {
    return (c > 0.0031308f) ? 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f : c * 12.92f;
}

static float labf(float t) {
    return (t > 0.008856f) ? std::cbrt(t) : (7.787f * t + 16.0f / 116.0f);
}

static float labfInv(float t) {
    return (t > 0.206893f) ? t * t * t : (t - 16.0f / 116.0f) / 7.787f;
}

static void rgbToLab(float r, float g, float b, float& L, float& a, float& labB) {
    // sRGB -> Linear
    float lr = gammaToLinear(std::clamp(r, 0.0f, 1.0f));
    float lg = gammaToLinear(std::clamp(g, 0.0f, 1.0f));
    float lb = gammaToLinear(std::clamp(b, 0.0f, 1.0f));

    // Linear RGB -> XYZ (sRGB matrix)
    float x = 0.4124564f * lr + 0.3575761f * lg + 0.1804375f * lb;
    float y = 0.2126729f * lr + 0.7151522f * lg + 0.0721750f * lb;
    float z = 0.0193339f * lr + 0.1191920f * lg + 0.9503041f * lb;

    // XYZ -> Lab
    float fx = labf(x / D65_X);
    float fy = labf(y / D65_Y);
    float fz = labf(z / D65_Z);

    L = 116.0f * fy - 16.0f;
    a = 500.0f * (fx - fy);
    labB = 200.0f * (fy - fz);
}

static void labToRgb(float L, float a, float labB, float& r, float& g, float& b) {
    // Lab -> XYZ
    float fy = (L + 16.0f) / 116.0f;
    float fx = a / 500.0f + fy;
    float fz = fy - labB / 200.0f;

    float x = D65_X * labfInv(fx);
    float y = D65_Y * labfInv(fy);
    float z = D65_Z * labfInv(fz);

    // XYZ -> Linear RGB
    float lr =  3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
    float lg = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
    float lb =  0.0556434f * x - 0.2040259f * y + 1.0572252f * z;

    // Linear -> sRGB
    r = linearToGamma(std::clamp(lr, 0.0f, 1.0f));
    g = linearToGamma(std::clamp(lg, 0.0f, 1.0f));
    b = linearToGamma(std::clamp(lb, 0.0f, 1.0f));
}

// ============================================================
// Internal: Channel statistics
// ============================================================

struct ChannelStats {
    double mean = 0.0;
    double stddev = 0.0;
};

static ChannelStats computeChannelStats(const float* data, int count) {
    ChannelStats s;
    double sum = 0.0;
    for (int i = 0; i < count; ++i) sum += data[i];
    s.mean = sum / std::max(1, count);
    double sqSum = 0.0;
    for (int i = 0; i < count; ++i) {
        double d = data[i] - s.mean;
        sqSum += d * d;
    }
    s.stddev = std::sqrt(sqSum / std::max(1, count));
    if (s.stddev < 0.0001) s.stddev = 0.0001;
    return s;
}

// ============================================================
// Reinhard Color Transfer
// ============================================================

void AutoColorMatcher::reinhardTransfer(float* srcPixels, int srcWidth, int srcHeight,
                                          const float* refPixels, int refWidth, int refHeight,
                                          float intensity) {
    const int srcTotal = srcWidth * srcHeight;
    const int refTotal = refWidth * refHeight;

    // Convert source to Lab
    std::vector<float> srcL(srcTotal), srcA(srcTotal), srcB(srcTotal);
    for (int i = 0; i < srcTotal; ++i) {
        int idx = i * 4;
        rgbToLab(srcPixels[idx], srcPixels[idx + 1], srcPixels[idx + 2],
                 srcL[i], srcA[i], srcB[i]);
    }

    // Convert reference to Lab
    std::vector<float> refL(refTotal), refA(refTotal), refB(refTotal);
    for (int i = 0; i < refTotal; ++i) {
        int idx = i * 4;
        rgbToLab(refPixels[idx], refPixels[idx + 1], refPixels[idx + 2],
                 refL[i], refA[i], refB[i]);
    }

    // Compute Lab statistics for both images
    auto srcStatsL = computeChannelStats(srcL.data(), srcTotal);
    auto srcStatsA = computeChannelStats(srcA.data(), srcTotal);
    auto srcStatsB = computeChannelStats(srcB.data(), srcTotal);

    auto refStatsL = computeChannelStats(refL.data(), refTotal);
    auto refStatsA = computeChannelStats(refA.data(), refTotal);
    auto refStatsB = computeChannelStats(refB.data(), refTotal);

    // Reinhard transfer: for each pixel
    // new_L = (src_L - src_mean_L) * (ref_std_L / src_std_L) + ref_mean_L
    for (int i = 0; i < srcTotal; ++i) {
        float newL = static_cast<float>(
            (srcL[i] - srcStatsL.mean) * (refStatsL.stddev / srcStatsL.stddev) + refStatsL.mean);
        float newA = static_cast<float>(
            (srcA[i] - srcStatsA.mean) * (refStatsA.stddev / srcStatsA.stddev) + refStatsA.mean);
        float newB_lab = static_cast<float>(
            (srcB[i] - srcStatsB.mean) * (refStatsB.stddev / srcStatsB.stddev) + refStatsB.mean);

        // Blend with original based on intensity
        float finalL = srcL[i] + (newL - srcL[i]) * intensity;
        float finalA = srcA[i] + (newA - srcA[i]) * intensity;
        float finalB_lab = srcB[i] + (newB_lab - srcB[i]) * intensity;

        // Convert back to RGB
        int idx = i * 4;
        float origR = srcPixels[idx], origG = srcPixels[idx + 1], origB = srcPixels[idx + 2];
        labToRgb(finalL, finalA, finalB_lab,
                 srcPixels[idx], srcPixels[idx + 1], srcPixels[idx + 2]);
    }
}

// ============================================================
// Mean/Stddev Match (RGB space — lightweight)
// ============================================================

void AutoColorMatcher::meanStddevMatch(float* srcPixels, int srcWidth, int srcHeight,
                                         const float* refPixels, int refWidth, int refHeight,
                                         float intensity) {
    const int srcTotal = srcWidth * srcHeight;
    const int refTotal = refWidth * refHeight;

    // Compute per-channel stats for source
    std::vector<float> srcR(srcTotal), srcG(srcTotal), srcBch(srcTotal);
    for (int i = 0; i < srcTotal; ++i) {
        int idx = i * 4;
        srcR[i] = srcPixels[idx]; srcG[i] = srcPixels[idx + 1]; srcBch[i] = srcPixels[idx + 2];
    }

    std::vector<float> refR(refTotal), refG(refTotal), refBch(refTotal);
    for (int i = 0; i < refTotal; ++i) {
        int idx = i * 4;
        refR[i] = refPixels[idx]; refG[i] = refPixels[idx + 1]; refBch[i] = refPixels[idx + 2];
    }

    auto sR = computeChannelStats(srcR.data(), srcTotal);
    auto sG = computeChannelStats(srcG.data(), srcTotal);
    auto sB = computeChannelStats(srcBch.data(), srcTotal);

    auto rR = computeChannelStats(refR.data(), refTotal);
    auto rG = computeChannelStats(refG.data(), refTotal);
    auto rB = computeChannelStats(refBch.data(), refTotal);

    for (int i = 0; i < srcTotal; ++i) {
        int idx = i * 4;
        float newR = static_cast<float>((srcPixels[idx]     - sR.mean) * (rR.stddev / sR.stddev) + rR.mean);
        float newG = static_cast<float>((srcPixels[idx + 1] - sG.mean) * (rG.stddev / sG.stddev) + rG.mean);
        float newB = static_cast<float>((srcPixels[idx + 2] - sB.mean) * (rB.stddev / sB.stddev) + rB.mean);

        srcPixels[idx]     = srcPixels[idx]     + (newR - srcPixels[idx])     * intensity;
        srcPixels[idx + 1] = srcPixels[idx + 1] + (newG - srcPixels[idx + 1]) * intensity;
        srcPixels[idx + 2] = srcPixels[idx + 2] + (newB - srcPixels[idx + 2]) * intensity;
    }
}

// ============================================================
// Histogram Matching
// ============================================================

static void buildCDF(const float* channel, int count, float cdf[256]) {
    int hist[256] = {};
    for (int i = 0; i < count; ++i) {
        int bin = std::clamp(static_cast<int>(channel[i] * 255.0f), 0, 255);
        hist[bin]++;
    }
    float invCount = 1.0f / std::max(1, count);
    cdf[0] = hist[0] * invCount;
    for (int i = 1; i < 256; ++i) {
        cdf[i] = cdf[i - 1] + hist[i] * invCount;
    }
}

static float matchCDF(float value, const float srcCDF[256], const float refCDF[256]) {
    int srcBin = std::clamp(static_cast<int>(value * 255.0f), 0, 255);
    float srcCDFVal = srcCDF[srcBin];
    
    // Find the bin in ref whose CDF is closest to srcCDFVal
    int bestBin = 0;
    float bestDist = 2.0f;
    for (int i = 0; i < 256; ++i) {
        float d = std::abs(refCDF[i] - srcCDFVal);
        if (d < bestDist) { bestDist = d; bestBin = i; }
    }
    return static_cast<float>(bestBin) / 255.0f;
}

void AutoColorMatcher::histogramMatch(float* srcPixels, int srcWidth, int srcHeight,
                                        const float* refPixels, int refWidth, int refHeight,
                                        float intensity) {
    const int srcTotal = srcWidth * srcHeight;
    const int refTotal = refWidth * refHeight;

    // Process each channel independently
    for (int ch = 0; ch < 3; ++ch) {
        std::vector<float> srcCh(srcTotal), refCh(refTotal);
        for (int i = 0; i < srcTotal; ++i) srcCh[i] = srcPixels[i * 4 + ch];
        for (int i = 0; i < refTotal; ++i) refCh[i] = refPixels[i * 4 + ch];

        float srcCDF[256], refCDF[256];
        buildCDF(srcCh.data(), srcTotal, srcCDF);
        buildCDF(refCh.data(), refTotal, refCDF);

        for (int i = 0; i < srcTotal; ++i) {
            float matched = matchCDF(srcPixels[i * 4 + ch], srcCDF, refCDF);
            srcPixels[i * 4 + ch] = srcPixels[i * 4 + ch] + (matched - srcPixels[i * 4 + ch]) * intensity;
        }
    }
}

// ============================================================
// Public API dispatchers
// ============================================================

void AutoColorMatcher::match(float* srcPixels, const float* refPixels,
                              int width, int height,
                              Method method, float intensity) {
    switch (method) {
    case Method::Reinhard:
        reinhardTransfer(srcPixels, width, height, refPixels, width, height, intensity);
        break;
    case Method::MeanStddev:
        meanStddevMatch(srcPixels, width, height, refPixels, width, height, intensity);
        break;
    case Method::Histogram:
        histogramMatch(srcPixels, width, height, refPixels, width, height, intensity);
        break;
    }
}

AutoColorMatcher::MatchResult AutoColorMatcher::computeMatch(
    const float* srcPixels, const float* refPixels,
    int srcWidth, int srcHeight,
    int refWidth, int refHeight,
    Method /*method*/)
{
    const int srcTotal = srcWidth * srcHeight;
    const int refTotal = refWidth * refHeight;
    MatchResult result;

    // Compute per-channel scales and offsets
    for (int ch = 0; ch < 3; ++ch) {
        std::vector<float> srcCh(srcTotal), refCh(refTotal);
        for (int i = 0; i < srcTotal; ++i) srcCh[i] = srcPixels[i * 4 + ch];
        for (int i = 0; i < refTotal; ++i) refCh[i] = refPixels[i * 4 + ch];

        auto srcStats = computeChannelStats(srcCh.data(), srcTotal);
        auto refStats = computeChannelStats(refCh.data(), refTotal);

        float scale = static_cast<float>(refStats.stddev / srcStats.stddev);
        float offset = static_cast<float>(refStats.mean - srcStats.mean * scale);

        switch (ch) {
        case 0: result.scaleR = scale; result.offsetR = offset; break;
        case 1: result.scaleG = scale; result.offsetG = offset; break;
        case 2: result.scaleB = scale; result.offsetB = offset; break;
        }
    }

    // Confidence based on how similar the stddevs already are
    float diffR = std::abs(result.scaleR - 1.0f);
    float diffG = std::abs(result.scaleG - 1.0f);
    float diffB = std::abs(result.scaleB - 1.0f);
    float avgDiff = (diffR + diffG + diffB) / 3.0f;
    result.confidence = std::clamp(1.0f - avgDiff, 0.0f, 1.0f);

    return result;
}

void AutoColorMatcher::applyMatch(float* pixels, int width, int height,
                                    const MatchResult& result, float intensity) {
    const int total = width * height;
    for (int i = 0; i < total; ++i) {
        int idx = i * 4;
        float r = pixels[idx] * result.scaleR + result.offsetR;
        float g = pixels[idx + 1] * result.scaleG + result.offsetG;
        float b = pixels[idx + 2] * result.scaleB + result.offsetB;

        pixels[idx]     = pixels[idx]     + (r - pixels[idx])     * intensity;
        pixels[idx + 1] = pixels[idx + 1] + (g - pixels[idx + 1]) * intensity;
        pixels[idx + 2] = pixels[idx + 2] + (b - pixels[idx + 2]) * intensity;
    }
}

} // namespace ArtifactCore
