module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
module Color.AutoMatch;

import Image.ImageF32x4_RGBA;
import Core.Parallel;
import Container.NamedVector;

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
    float lr = gammaToLinear(std::clamp(r, 0.0f, 1.0f));
    float lg = gammaToLinear(std::clamp(g, 0.0f, 1.0f));
    float lb = gammaToLinear(std::clamp(b, 0.0f, 1.0f));

    float x = 0.4124564f * lr + 0.3575761f * lg + 0.1804375f * lb;
    float y = 0.2126729f * lr + 0.7151522f * lg + 0.0721750f * lb;
    float z = 0.0193339f * lr + 0.1191920f * lg + 0.9503041f * lb;

    float fx = labf(x / D65_X);
    float fy = labf(y / D65_Y);
    float fz = labf(z / D65_Z);

    L = 116.0f * fy - 16.0f;
    a = 500.0f * (fx - fy);
    labB = 200.0f * (fy - fz);
}

static void labToRgb(float L, float a, float labB, float& r, float& g, float& b) {
    float fy = (L + 16.0f) / 116.0f;
    float fx = a / 500.0f + fy;
    float fz = fy - labB / 200.0f;

    float x = D65_X * labfInv(fx);
    float y = D65_Y * labfInv(fy);
    float z = D65_Z * labfInv(fz);

    float lr =  3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
    float lg = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
    float lb =  0.0556434f * x - 0.2040259f * y + 1.0572252f * z;

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

static std::size_t checkedRgbaPixelCount(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return 0;
    const auto widthSize = static_cast<std::size_t>(width);
    const auto heightSize = static_cast<std::size_t>(height);
    if (widthSize > std::numeric_limits<std::size_t>::max() / heightSize) {
        return 0;
    }
    const auto count = widthSize * heightSize;
    return count > std::numeric_limits<std::size_t>::max() / 4u ? 0 : count;
}

static ChannelStats computeChannelStats(const float* data, std::size_t count) {
    ChannelStats s;
    constexpr std::size_t kChunkSize = 4096;
    const std::size_t chunkCount = count / kChunkSize +
                                   (count % kChunkSize != 0 ? 1u : 0u);
    std::vector<double> partialSums;
    partialSums.resize(chunkCount);
    Parallel::ForSize(0, chunkCount, count, [&](std::size_t chunk) {
        const std::size_t begin = chunk * kChunkSize;
        const std::size_t end = begin + std::min(kChunkSize, count - begin);
        double sum = 0.0;
        for (std::size_t i = begin; i < end; ++i) sum += data[i];
        partialSums[chunk] = sum;
    });

    double sum = 0.0;
    for (double partial : partialSums) sum += partial;
    s.mean = sum / static_cast<double>(std::max<std::size_t>(1, count));

    std::vector<double> partialSquaredSums;
    partialSquaredSums.resize(chunkCount);
    Parallel::ForSize(0, chunkCount, count, [&](std::size_t chunk) {
        const std::size_t begin = chunk * kChunkSize;
        const std::size_t end = begin + std::min(kChunkSize, count - begin);
        double sqSum = 0.0;
        for (std::size_t i = begin; i < end; ++i) {
            const double d = data[i] - s.mean;
            sqSum += d * d;
        }
        partialSquaredSums[chunk] = sqSum;
    });

    double sqSum = 0.0;
    for (double partial : partialSquaredSums) sqSum += partial;
    s.stddev = std::sqrt(
        sqSum / static_cast<double>(std::max<std::size_t>(1, count)));
    if (s.stddev < 0.0001) s.stddev = 0.0001;
    return s;
}

// ============================================================
// Reinhard Color Transfer
// ============================================================

void AutoColorMatcher::reinhardTransfer(float* srcPixels, int srcWidth, int srcHeight,
                                          const float* refPixels, int refWidth, int refHeight,
                                          float intensity) {
    const std::size_t srcTotal = checkedRgbaPixelCount(srcWidth, srcHeight);
    const std::size_t refTotal = checkedRgbaPixelCount(refWidth, refHeight);
    if (srcTotal == 0 || refTotal == 0 || !srcPixels || !refPixels) return;

    std::vector<float> srcL, srcA, srcB;
    srcL.resize(srcTotal);
    srcA.resize(srcTotal);
    srcB.resize(srcTotal);
    Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
        const std::size_t idx = i * 4u;
        rgbToLab(srcPixels[idx], srcPixels[idx + 1], srcPixels[idx + 2],
                 srcL[i], srcA[i], srcB[i]);
    });

    std::vector<float> refL, refA, refB;
    refL.resize(refTotal);
    refA.resize(refTotal);
    refB.resize(refTotal);
    Parallel::ForSize(0, refTotal, refTotal, [&](std::size_t i) {
        const std::size_t idx = i * 4u;
        rgbToLab(refPixels[idx], refPixels[idx + 1], refPixels[idx + 2],
                 refL[i], refA[i], refB[i]);
    });

    auto srcStatsL = computeChannelStats(srcL.data(), srcTotal);
    auto srcStatsA = computeChannelStats(srcA.data(), srcTotal);
    auto srcStatsB = computeChannelStats(srcB.data(), srcTotal);

    auto refStatsL = computeChannelStats(refL.data(), refTotal);
    auto refStatsA = computeChannelStats(refA.data(), refTotal);
    auto refStatsB = computeChannelStats(refB.data(), refTotal);

    Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
        float newL = static_cast<float>(
            (srcL[i] - srcStatsL.mean) * (refStatsL.stddev / srcStatsL.stddev) + refStatsL.mean);
        float newA = static_cast<float>(
            (srcA[i] - srcStatsA.mean) * (refStatsA.stddev / srcStatsA.stddev) + refStatsA.mean);
        float newB_lab = static_cast<float>(
            (srcB[i] - srcStatsB.mean) * (refStatsB.stddev / srcStatsB.stddev) + refStatsB.mean);

        float finalL = srcL[i] + (newL - srcL[i]) * intensity;
        float finalA = srcA[i] + (newA - srcA[i]) * intensity;
        float finalB_lab = srcB[i] + (newB_lab - srcB[i]) * intensity;

        const std::size_t idx = i * 4u;
        labToRgb(finalL, finalA, finalB_lab,
                 srcPixels[idx], srcPixels[idx + 1], srcPixels[idx + 2]);
    });
}

// ============================================================
// Mean/Stddev Match
// ============================================================

void AutoColorMatcher::meanStddevMatch(float* srcPixels, int srcWidth, int srcHeight,
                                         const float* refPixels, int refWidth, int refHeight,
                                         float intensity) {
    const std::size_t srcTotal = checkedRgbaPixelCount(srcWidth, srcHeight);
    const std::size_t refTotal = checkedRgbaPixelCount(refWidth, refHeight);
    if (srcTotal == 0 || refTotal == 0 || !srcPixels || !refPixels) return;

    std::vector<float> srcR, srcG, srcBch;
    srcR.resize(srcTotal);
    srcG.resize(srcTotal);
    srcBch.resize(srcTotal);
    Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
        const std::size_t idx = i * 4u;
        srcR[i] = srcPixels[idx]; srcG[i] = srcPixels[idx + 1]; srcBch[i] = srcPixels[idx + 2];
    });

    std::vector<float> refR, refG, refBch;
    refR.resize(refTotal);
    refG.resize(refTotal);
    refBch.resize(refTotal);
    Parallel::ForSize(0, refTotal, refTotal, [&](std::size_t i) {
        const std::size_t idx = i * 4u;
        refR[i] = refPixels[idx]; refG[i] = refPixels[idx + 1]; refBch[i] = refPixels[idx + 2];
    });

    auto sR = computeChannelStats(srcR.data(), srcTotal);
    auto sG = computeChannelStats(srcG.data(), srcTotal);
    auto sB = computeChannelStats(srcBch.data(), srcTotal);

    auto rR = computeChannelStats(refR.data(), refTotal);
    auto rG = computeChannelStats(refG.data(), refTotal);
    auto rB = computeChannelStats(refBch.data(), refTotal);

    Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
        const std::size_t idx = i * 4u;
        float newR = static_cast<float>((srcPixels[idx]     - sR.mean) * (rR.stddev / sR.stddev) + rR.mean);
        float newG = static_cast<float>((srcPixels[idx + 1] - sG.mean) * (rG.stddev / sG.stddev) + rG.mean);
        float newB = static_cast<float>((srcPixels[idx + 2] - sB.mean) * (rB.stddev / sB.stddev) + rB.mean);

        srcPixels[idx]     = srcPixels[idx]     + (newR - srcPixels[idx])     * intensity;
        srcPixels[idx + 1] = srcPixels[idx + 1] + (newG - srcPixels[idx + 1]) * intensity;
        srcPixels[idx + 2] = srcPixels[idx + 2] + (newB - srcPixels[idx + 2]) * intensity;
    });
}

// ============================================================
// Histogram Matching
// ============================================================

static void buildCDF(const float* channel, std::size_t count, float cdf[256]) {
    constexpr std::size_t kChunkSize = 4096;
    const std::size_t chunkCount = count / kChunkSize +
                                   (count % kChunkSize != 0 ? 1u : 0u);
    std::vector<std::array<int, 256>> partialHist;
    partialHist.resize(chunkCount);
    Parallel::ForSize(0, chunkCount, count, [&](std::size_t chunk) {
        auto& hist = partialHist[chunk];
        hist.fill(0);
        const std::size_t begin = chunk * kChunkSize;
        const std::size_t end = begin + std::min(kChunkSize, count - begin);
        for (std::size_t i = begin; i < end; ++i) {
            const int bin = std::clamp(static_cast<int>(channel[i] * 255.0f), 0, 255);
            ++hist[static_cast<size_t>(bin)];
        }
    });

    std::array<std::uint64_t, 256> hist{};
    for (const auto& partial : partialHist) {
        for (int bin = 0; bin < 256; ++bin) {
            hist[static_cast<std::size_t>(bin)] +=
                static_cast<std::uint64_t>(partial[static_cast<std::size_t>(bin)]);
        }
    }
    const double invCount = 1.0 / static_cast<double>(
        std::max<std::size_t>(1, count));
    cdf[0] = static_cast<float>(static_cast<double>(hist[0]) * invCount);
    for (int i = 1; i < 256; ++i) {
        cdf[i] = cdf[i - 1] + static_cast<float>(
            static_cast<double>(hist[static_cast<std::size_t>(i)]) * invCount);
    }
}

static float matchCDF(float value, const float srcCDF[256], const float refCDF[256]) {
    int srcBin = std::clamp(static_cast<int>(value * 255.0f), 0, 255);
    float srcCDFVal = srcCDF[srcBin];

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
    const std::size_t srcTotal = checkedRgbaPixelCount(srcWidth, srcHeight);
    const std::size_t refTotal = checkedRgbaPixelCount(refWidth, refHeight);
    if (srcTotal == 0 || refTotal == 0 || !srcPixels || !refPixels) return;

    for (int ch = 0; ch < 3; ++ch) {
        std::vector<float> srcCh, refCh;
        srcCh.resize(srcTotal);
        refCh.resize(refTotal);
        Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
            srcCh[i] = srcPixels[i * 4u + static_cast<std::size_t>(ch)];
        });
        Parallel::ForSize(0, refTotal, refTotal, [&](std::size_t i) {
            refCh[i] = refPixels[i * 4u + static_cast<std::size_t>(ch)];
        });

        float srcCDF[256], refCDF[256];
        buildCDF(srcCh.data(), srcTotal, srcCDF);
        buildCDF(refCh.data(), refTotal, refCDF);

        Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
            const std::size_t index = i * 4u + static_cast<std::size_t>(ch);
            float matched = matchCDF(srcPixels[index], srcCDF, refCDF);
            srcPixels[index] = srcPixels[index] + (matched - srcPixels[index]) * intensity;
        });
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
    MatchResult result;
    const std::size_t srcTotal = checkedRgbaPixelCount(srcWidth, srcHeight);
    const std::size_t refTotal = checkedRgbaPixelCount(refWidth, refHeight);
    if (srcTotal == 0 || refTotal == 0 || !srcPixels || !refPixels) {
        return result;
    }

    for (int ch = 0; ch < 3; ++ch) {
        std::vector<float> srcCh, refCh;
        srcCh.resize(srcTotal);
        refCh.resize(refTotal);
        Parallel::ForSize(0, srcTotal, srcTotal, [&](std::size_t i) {
            srcCh[i] = srcPixels[i * 4u + static_cast<std::size_t>(ch)];
        });
        Parallel::ForSize(0, refTotal, refTotal, [&](std::size_t i) {
            refCh[i] = refPixels[i * 4u + static_cast<std::size_t>(ch)];
        });

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

    float diffR = std::abs(result.scaleR - 1.0f);
    float diffG = std::abs(result.scaleG - 1.0f);
    float diffB = std::abs(result.scaleB - 1.0f);
    float avgDiff = (diffR + diffG + diffB) / 3.0f;
    result.confidence = std::clamp(1.0f - avgDiff, 0.0f, 1.0f);

    return result;
}

void AutoColorMatcher::applyMatch(float* pixels, int width, int height,
                                    const MatchResult& result, float intensity) {
    const std::size_t total = checkedRgbaPixelCount(width, height);
    if (total == 0 || !pixels) return;
    Parallel::ForSize(0, total, total, [&](std::size_t i) {
        const std::size_t idx = i * 4u;
        float r = pixels[idx] * result.scaleR + result.offsetR;
        float g = pixels[idx + 1] * result.scaleG + result.offsetG;
        float b = pixels[idx + 2] * result.scaleB + result.offsetB;

        pixels[idx]     = pixels[idx]     + (r - pixels[idx])     * intensity;
        pixels[idx + 1] = pixels[idx + 1] + (g - pixels[idx + 1]) * intensity;
        pixels[idx + 2] = pixels[idx + 2] + (b - pixels[idx + 2]) * intensity;
    });
}

// ============================================================
// ImageF32x4_RGBA overloads
// ============================================================

void AutoColorMatcher::match(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                              Method method, float intensity)
{
    match(src.rgba32fData(), ref.rgba32fData(), src.width(), src.height(), method, intensity);
}

void AutoColorMatcher::reinhardTransfer(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                                         float intensity)
{
    reinhardTransfer(src.rgba32fData(), src.width(), src.height(),
                     ref.rgba32fData(), ref.width(), ref.height(), intensity);
}

void AutoColorMatcher::meanStddevMatch(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                                        float intensity)
{
    meanStddevMatch(src.rgba32fData(), src.width(), src.height(),
                    ref.rgba32fData(), ref.width(), ref.height(), intensity);
}

void AutoColorMatcher::histogramMatch(ImageF32x4_RGBA& src, const ImageF32x4_RGBA& ref,
                                       float intensity)
{
    histogramMatch(src.rgba32fData(), src.width(), src.height(),
                   ref.rgba32fData(), ref.width(), ref.height(), intensity);
}

} // namespace ArtifactCore
