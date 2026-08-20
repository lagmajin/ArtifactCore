module;
#include <algorithm>
#include <cmath>

module Core.AI.ImageDepthEstimator;

namespace ArtifactCore {

DepthMap LuminanceImageDepthEstimator::estimate(
    const ImageF32x4_RGBA& image, const ImageDepthEstimateOptions& options)
{
    if (image.isEmpty()) return {};
    const int sourceW = image.width();
    const int sourceH = image.height();
    const int limit = std::max(2, options.maxDimension);
    const float scale = std::min(1.0f, static_cast<float>(limit) /
                                        static_cast<float>(std::max(sourceW, sourceH)));
    const int width = std::max(2, static_cast<int>(std::round(sourceW * scale)));
    const int height = std::max(2, static_cast<int>(std::round(sourceH * scale)));
    DepthMap result(width, height);
    for (int y = 0; y < height; ++y) {
        const int sy = std::min(sourceH - 1,
                                static_cast<int>(std::round(static_cast<float>(y) /
                                                            static_cast<float>(height - 1) * (sourceH - 1))));
        for (int x = 0; x < width; ++x) {
            const int sx = std::min(sourceW - 1,
                                    static_cast<int>(std::round(static_cast<float>(x) /
                                                                static_cast<float>(width - 1) * (sourceW - 1))));
            const auto pixel = image.getPixel(sx, sy);
            result.set(x, y, std::clamp(0.2126f * pixel.r() +
                                        0.7152f * pixel.g() +
                                        0.0722f * pixel.b(), 0.0f, 1.0f));
        }
    }
    if (options.normalize) result.normalize();
    if (options.invert) result.invert();
    return result;
}

}
