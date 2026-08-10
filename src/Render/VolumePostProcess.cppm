module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

module Render.VolumePostProcess;

namespace ArtifactCore::RayTrace {

namespace {

inline float clamp01(float v) noexcept {
    return std::clamp(v, 0.0f, 1.0f);
}

inline float luminance(const Color& c) noexcept {
    return c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f;
}

}

void VolumePostProcessor::setSettings(const VolumePostProcessSettings& settings) {
    settings_ = settings;
}

float VolumePostProcessor::pixelLuminance(const ImageBuffer& image, int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return 0.0f;
    const auto offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) * 3ull + static_cast<std::size_t>(x) * 3ull;
    const Color c{
        static_cast<float>(image.pixels[offset + 0]) / 255.0f,
        static_cast<float>(image.pixels[offset + 1]) / 255.0f,
        static_cast<float>(image.pixels[offset + 2]) / 255.0f,
    };
    return luminance(c);
}

Color VolumePostProcessor::pixelColor(const ImageBuffer& image, int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return {0, 0, 0};
    const auto offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) * 3ull + static_cast<std::size_t>(x) * 3ull;
    return {
        static_cast<float>(image.pixels[offset + 0]) / 255.0f,
        static_cast<float>(image.pixels[offset + 1]) / 255.0f,
        static_cast<float>(image.pixels[offset + 2]) / 255.0f,
    };
}

void VolumePostProcessor::setPixel(ImageBuffer& image, int x, int y, const Color& color) const noexcept {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
    const auto offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) * 3ull + static_cast<std::size_t>(x) * 3ull;
    image.pixels[offset + 0] = static_cast<std::uint8_t>(std::clamp(color.x * 255.999f, 0.0f, 255.0f));
    image.pixels[offset + 1] = static_cast<std::uint8_t>(std::clamp(color.y * 255.999f, 0.0f, 255.0f));
    image.pixels[offset + 2] = static_cast<std::uint8_t>(std::clamp(color.z * 255.999f, 0.0f, 255.0f));
}

void VolumePostProcessor::process(ImageBuffer& image) const noexcept {
    const auto requiredBytes = imagePixelBytes(image.width, image.height);
    if (requiredBytes == 0 || image.pixels.size() < requiredBytes) {
        return;
    }
    if (settings_.denoise.enabled) {
        applyBilateralFilter(image);
    }
    applyBloom(image);
    applyGlare(image);
    applyExposureGamma(image);
}

void VolumePostProcessor::applyBloom(ImageBuffer& image) const noexcept {
    if (!settings_.bloom.enabled) return;

    const int w = image.width;
    const int h = image.height;
    const auto& bloom = settings_.bloom;

    std::vector<float> lum(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) {
        const auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        float* lumRow = lum.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        for (int x = 0; x < w; ++x) {
            const float l = (static_cast<float>(row[x * 3 + 0]) * 0.2126f +
                             static_cast<float>(row[x * 3 + 1]) * 0.7152f +
                             static_cast<float>(row[x * 3 + 2]) * 0.0722f) / 255.0f;
            lumRow[x] = std::max(0.0f, l - bloom.threshold);
        }
    }

    const int radius = std::max(1, static_cast<int>(bloom.radius * static_cast<float>(std::min(w, h))));
    std::vector<float> blurred(lum.size());

    for (int iter = 0; iter < bloom.iterations; ++iter) {
        for (int y = 0; y < h; ++y) {
            float* blurredRow = blurred.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
            for (int x = 0; x < w; ++x) {
                float sum = 0.0f;
                int count = 0;
                for (int dy = -radius; dy <= radius; ++dy) {
                    const int ny = y + dy;
                    if (ny < 0 || ny >= h) continue;
                    const float* lumRow = lum.data() + static_cast<std::size_t>(ny) * static_cast<std::size_t>(w);
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int nx = x + dx;
                        if (nx < 0 || nx >= w) continue;
                        sum += lumRow[nx];
                        ++count;
                    }
                }
                blurredRow[x] = sum / static_cast<float>(count);
            }
        }
        for (std::size_t i = 0; i < lum.size(); ++i) {
            lum[i] = blurred[i];
        }
    }

    for (int y = 0; y < h; ++y) {
        auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        const float* blurredRow = blurred.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        for (int x = 0; x < w; ++x) {
            const float add = blurredRow[x] * bloom.intensity;
            row[x * 3 + 0] = static_cast<std::uint8_t>(clamp01(row[x * 3 + 0] / 255.0f + add) * 255.999f);
            row[x * 3 + 1] = static_cast<std::uint8_t>(clamp01(row[x * 3 + 1] / 255.0f + add) * 255.999f);
            row[x * 3 + 2] = static_cast<std::uint8_t>(clamp01(row[x * 3 + 2] / 255.0f + add) * 255.999f);
        }
    }
}

void VolumePostProcessor::applyGlare(ImageBuffer& image) const noexcept {
    if (!settings_.glare.enabled) return;

    const int w = image.width;
    const int h = image.height;
    const auto& glare = settings_.glare;

    const float brightnessThreshold = 0.8f;
    std::vector<float> lum(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) {
        const auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        for (int x = 0; x < w; ++x) {
            const float l = (static_cast<float>(row[x * 3 + 0]) * 0.2126f +
                             static_cast<float>(row[x * 3 + 1]) * 0.7152f +
                             static_cast<float>(row[x * 3 + 2]) * 0.0722f) / 255.0f;
            lum[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = std::max(0.0f, l - brightnessThreshold);
        }
    }

    const float baseAngle = glare.angleOffset * 3.14159265f / 180.0f;
    const int streakLen = std::max(1, static_cast<int>(glare.streakLength * static_cast<float>(std::max(w, h))));
    const int streakCount = std::max(0, glare.streakCount);
    std::vector<float> streakCos(static_cast<std::size_t>(streakCount));
    std::vector<float> streakSin(static_cast<std::size_t>(streakCount));
    for (int i = 0; i < streakCount; ++i) {
        const float angle = baseAngle + static_cast<float>(i) * 2.0f * 3.14159265f /
                            static_cast<float>(streakCount);
        streakCos[static_cast<std::size_t>(i)] = std::cos(angle);
        streakSin[static_cast<std::size_t>(i)] = std::sin(angle);
    }
    std::vector<float> streakFalloff(static_cast<std::size_t>(streakLen + 1));
    for (int s = 1; s <= streakLen; ++s) {
        streakFalloff[static_cast<std::size_t>(s)] =
            1.0f - static_cast<float>(s) / static_cast<float>(streakLen + 1);
    }

    for (int y = 0; y < h; ++y) {
        auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        for (int x = 0; x < w; ++x) {
            float streakAccum = 0.0f;
            for (int i = 0; i < streakCount; ++i) {
                for (int s = 1; s <= streakLen; ++s) {
                    const int sx = x + static_cast<int>(streakCos[static_cast<std::size_t>(i)] * static_cast<float>(s));
                    const int sy = y + static_cast<int>(streakSin[static_cast<std::size_t>(i)] * static_cast<float>(s));
                    if (sx < 0 || sy < 0 || sx >= w || sy >= h) break;
                    const float* lumRow = lum.data() + static_cast<std::size_t>(sy) * static_cast<std::size_t>(w);
                    streakAccum += lumRow[sx] * streakFalloff[static_cast<std::size_t>(s)];
                }
            }
            const float add = streakAccum * glare.intensity;
            row[x * 3 + 0] = static_cast<std::uint8_t>(clamp01(row[x * 3 + 0] / 255.0f + add) * 255.999f);
            row[x * 3 + 1] = static_cast<std::uint8_t>(clamp01(row[x * 3 + 1] / 255.0f + add * 0.9f) * 255.999f);
            row[x * 3 + 2] = static_cast<std::uint8_t>(clamp01(row[x * 3 + 2] / 255.0f + add * 0.7f) * 255.999f);
        }
    }
}

void VolumePostProcessor::applyBilateralFilter(ImageBuffer& image) const noexcept {
    const int w = image.width;
    const int h = image.height;
    const auto& dn = settings_.denoise;

    std::vector<float> origR(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    std::vector<float> origG(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    std::vector<float> origB(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));

    for (int y = 0; y < h; ++y) {
        const auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        float* origRRow = origR.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float* origGRow = origG.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        float* origBRow = origB.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        for (int x = 0; x < w; ++x) {
            origRRow[x] = static_cast<float>(row[x * 3 + 0]) / 255.0f;
            origGRow[x] = static_cast<float>(row[x * 3 + 1]) / 255.0f;
            origBRow[x] = static_cast<float>(row[x * 3 + 2]) / 255.0f;
        }
    }

    const float safeSpatialSigma = std::max(std::abs(dn.spatialSigma), 1.0e-6f);
    const float safeRangeSigma = std::max(std::abs(dn.rangeSigma), 1.0e-6f);
    const float spatialDenom = 2.0f * safeSpatialSigma * safeSpatialSigma;
    const float rangeDenom = 2.0f * safeRangeSigma * safeRangeSigma;
    const int filterRadius = dn.filterRadius;
    const int kernelRadius = std::max(0, filterRadius);
    const int kernelSize = kernelRadius * 2 + 1;
    std::vector<float> spatialWeights(static_cast<std::size_t>(kernelSize) *
                                      static_cast<std::size_t>(kernelSize));
    for (int dy = -kernelRadius; dy <= kernelRadius; ++dy) {
        for (int dx = -kernelRadius; dx <= kernelRadius; ++dx) {
            spatialWeights[static_cast<std::size_t>(dy + kernelRadius) *
                           static_cast<std::size_t>(kernelSize) +
                           static_cast<std::size_t>(dx + kernelRadius)] =
                std::exp(-static_cast<float>(dx * dx + dy * dy) / spatialDenom);
        }
    }

    for (int y = 0; y < h; ++y) {
        auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        const float* origRRow = origR.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        const float* origGRow = origG.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        const float* origBRow = origB.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        for (int x = 0; x < w; ++x) {
            const float centerR = origRRow[x];
            const float centerG = origGRow[x];
            const float centerB = origBRow[x];

            float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
            float weightSum = 0.0f;

            for (int dy = -kernelRadius; dy <= kernelRadius; ++dy) {
                const int ny = y + dy;
                if (ny < 0 || ny >= h) continue;
                const float* origRNeighbor = origR.data() + static_cast<std::size_t>(ny) * static_cast<std::size_t>(w);
                const float* origGNeighbor = origG.data() + static_cast<std::size_t>(ny) * static_cast<std::size_t>(w);
                const float* origBNeighbor = origB.data() + static_cast<std::size_t>(ny) * static_cast<std::size_t>(w);
                for (int dx = -kernelRadius; dx <= kernelRadius; ++dx) {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= w) continue;

                    const float spatialWeight = spatialWeights[
                        static_cast<std::size_t>(dy + kernelRadius) *
                        static_cast<std::size_t>(kernelSize) +
                        static_cast<std::size_t>(dx + kernelRadius)];

                    const float dr = origRNeighbor[nx] - centerR;
                    const float dg = origGNeighbor[nx] - centerG;
                    const float db = origBNeighbor[nx] - centerB;
                    const float rangeWeight = std::exp(-(dr * dr + dg * dg + db * db) / rangeDenom);

                    const float weight = spatialWeight * rangeWeight;
                    sumR += origRNeighbor[nx] * weight;
                    sumG += origGNeighbor[nx] * weight;
                    sumB += origBNeighbor[nx] * weight;
                    weightSum += weight;
                }
            }

            const float invWeight = 1.0f / std::max(weightSum, 1e-10f);
            row[x * 3 + 0] = static_cast<std::uint8_t>(std::clamp(sumR * invWeight * 255.999f, 0.0f, 255.0f));
            row[x * 3 + 1] = static_cast<std::uint8_t>(std::clamp(sumG * invWeight * 255.999f, 0.0f, 255.0f));
            row[x * 3 + 2] = static_cast<std::uint8_t>(std::clamp(sumB * invWeight * 255.999f, 0.0f, 255.0f));
        }
    }
}

void VolumePostProcessor::applyExposureGamma(ImageBuffer& image) const noexcept {
    const int w = image.width;
    const int h = image.height;
    const float exposure = settings_.exposure;
    const float inverseGamma = 1.0f / settings_.gamma;

    for (int y = 0; y < h; ++y) {
        auto* row = image.pixels.data() + static_cast<std::size_t>(y) * w * 3u;
        for (int x = 0; x < w; ++x) {
            row[x * 3 + 0] = static_cast<std::uint8_t>(std::pow(clamp01(row[x * 3 + 0] / 255.0f * exposure), inverseGamma) * 255.999f);
            row[x * 3 + 1] = static_cast<std::uint8_t>(std::pow(clamp01(row[x * 3 + 1] / 255.0f * exposure), inverseGamma) * 255.999f);
            row[x * 3 + 2] = static_cast<std::uint8_t>(std::pow(clamp01(row[x * 3 + 2] / 255.0f * exposure), inverseGamma) * 255.999f);
        }
    }
}

} // namespace ArtifactCore::RayTrace
