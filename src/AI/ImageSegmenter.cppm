module;
#include <algorithm>
#include <cmath>

module Core.AI.ImageSegmenter;

import FloatRGBA;

namespace {
float luminance(const ArtifactCore::FloatRGBA& pixel)
{
    return std::clamp(0.2126f * pixel.r() + 0.7152f * pixel.g() +
                      0.0722f * pixel.b(), 0.0f, 1.0f);
}
}

namespace ArtifactCore {

DepthMap LuminanceImageSegmenter::segment(
    const ImageF32x4_RGBA& image, const ImageSegmentationOptions& options)
{
    if (image.isEmpty()) return {};
    const int maxDimension = std::max(2, options.maxDimension);
    const float scale = std::min(1.0f, static_cast<float>(maxDimension) /
                                        static_cast<float>(std::max(image.width(), image.height())));
    const int width = std::max(2, static_cast<int>(std::round(image.width() * scale)));
    const int height = std::max(2, static_cast<int>(std::round(image.height() * scale)));
    DepthMap result(width, height);
    for (int y = 0; y < height; ++y) {
        const int sy = std::min(image.height() - 1,
                                static_cast<int>(std::round(static_cast<float>(y) /
                                                            std::max(1, height - 1) * (image.height() - 1))));
        for (int x = 0; x < width; ++x) {
            const int sx = std::min(image.width() - 1,
                                    static_cast<int>(std::round(static_cast<float>(x) /
                                                                std::max(1, width - 1) * (image.width() - 1))));
            const float value = luminance(image.getPixel(sx, sy));
            result.set(x, y, value >= options.threshold ? 1.0f : 0.0f);
        }
    }
    if (options.invert) result.invert();
    return result;
}

void applySegmentationMask(ImageF32x4_RGBA& image, const DepthMap& mask,
                           float opacity)
{
    if (image.isEmpty() || mask.isEmpty()) return;
    const float alphaScale = std::clamp(opacity, 0.0f, 1.0f);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            FloatRGBA pixel = image.getPixel(x, y);
            const float u = image.width() > 1
                ? static_cast<float>(x) / static_cast<float>(image.width() - 1) : 0.0f;
            const float v = image.height() > 1
                ? static_cast<float>(y) / static_cast<float>(image.height() - 1) : 0.0f;
            pixel.setAlpha(pixel.a() * mask.sampleBilinear(u, v) * alphaScale);
            image.setPixel(x, y, pixel);
        }
    }
}

}
