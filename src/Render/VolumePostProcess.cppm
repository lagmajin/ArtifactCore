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
        for (int x = 0; x < w; ++x) {
            const float l = pixelLuminance(image, x, y);
            lum[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = std::max(0.0f, l - bloom.threshold);
        }
    }

    const int radius = std::max(1, static_cast<int>(bloom.radius * static_cast<float>(std::min(w, h))));
    std::vector<float> blurred(lum.size());

    for (int iter = 0; iter < bloom.iterations; ++iter) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float sum = 0.0f;
                int count = 0;
                for (int dy = -radius; dy <= radius; ++dy) {
                    const int ny = y + dy;
                    if (ny < 0 || ny >= h) continue;
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int nx = x + dx;
                        if (nx < 0 || nx >= w) continue;
                        sum += lum[static_cast<std::size_t>(ny) * static_cast<std::size_t>(w) + static_cast<std::size_t>(nx)];
                        ++count;
                    }
                }
                blurred[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = sum / static_cast<float>(count);
            }
        }
        std::copy(blurred.begin(), blurred.end(), lum.begin());
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float add = blurred[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] * bloom.intensity;
            auto c = pixelColor(image, x, y);
            c.x = clamp01(c.x + add);
            c.y = clamp01(c.y + add);
            c.z = clamp01(c.z + add);
            setPixel(image, x, y, c);
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
        for (int x = 0; x < w; ++x) {
            lum[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = std::max(0.0f, pixelLuminance(image, x, y) - brightnessThreshold);
        }
    }

    const float baseAngle = glare.angleOffset * 3.14159265f / 180.0f;
    const int streakLen = std::max(1, static_cast<int>(glare.streakLength * static_cast<float>(std::max(w, h))));

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float streakAccum = 0.0f;
            for (int i = 0; i < glare.streakCount; ++i) {
                const float angle = baseAngle + static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(glare.streakCount);
                for (int s = 1; s <= streakLen; ++s) {
                    const int sx = x + static_cast<int>(std::cos(angle) * static_cast<float>(s));
                    const int sy = y + static_cast<int>(std::sin(angle) * static_cast<float>(s));
                    if (sx < 0 || sy < 0 || sx >= w || sy >= h) break;
                    const float falloff = 1.0f - static_cast<float>(s) / static_cast<float>(streakLen + 1);
                    streakAccum += lum[static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(sx)] * falloff;
                }
            }
            const float add = streakAccum * glare.intensity;
            auto c = pixelColor(image, x, y);
            c.x = clamp01(c.x + add);
            c.y = clamp01(c.y + add * 0.9f);
            c.z = clamp01(c.z + add * 0.7f);
            setPixel(image, x, y, c);
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
        for (int x = 0; x < w; ++x) {
            const auto c = pixelColor(image, x, y);
            const auto idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            origR[idx] = c.x;
            origG[idx] = c.y;
            origB[idx] = c.z;
        }
    }

    const float spatialDenom = 2.0f * dn.spatialSigma * dn.spatialSigma;
    const float rangeDenom = 2.0f * dn.rangeSigma * dn.rangeSigma;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const auto centerIdx = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            const float centerR = origR[centerIdx];
            const float centerG = origG[centerIdx];
            const float centerB = origB[centerIdx];

            float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
            float weightSum = 0.0f;

            for (int dy = -dn.filterRadius; dy <= dn.filterRadius; ++dy) {
                const int ny = y + dy;
                if (ny < 0 || ny >= h) continue;
                for (int dx = -dn.filterRadius; dx <= dn.filterRadius; ++dx) {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= w) continue;

                    const float spatialWeight = std::exp(-static_cast<float>(dx * dx + dy * dy) / spatialDenom);

                    const auto idx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(w) + static_cast<std::size_t>(nx);
                    const float dr = origR[idx] - centerR;
                    const float dg = origG[idx] - centerG;
                    const float db = origB[idx] - centerB;
                    const float rangeWeight = std::exp(-(dr * dr + dg * dg + db * db) / rangeDenom);

                    const float weight = spatialWeight * rangeWeight;
                    sumR += origR[idx] * weight;
                    sumG += origG[idx] * weight;
                    sumB += origB[idx] * weight;
                    weightSum += weight;
                }
            }

            setPixel(image, x, y, {
                sumR / std::max(weightSum, 1e-10f),
                sumG / std::max(weightSum, 1e-10f),
                sumB / std::max(weightSum, 1e-10f)
            });
        }
    }
}

void VolumePostProcessor::applyExposureGamma(ImageBuffer& image) const noexcept {
    const int w = image.width;
    const int h = image.height;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            auto c = pixelColor(image, x, y);
            c.x = std::pow(clamp01(c.x * settings_.exposure), 1.0f / settings_.gamma);
            c.y = std::pow(clamp01(c.y * settings_.exposure), 1.0f / settings_.gamma);
            c.z = std::pow(clamp01(c.z * settings_.exposure), 1.0f / settings_.gamma);
            setPixel(image, x, y, c);
        }
    }
}

} // namespace ArtifactCore::RayTrace