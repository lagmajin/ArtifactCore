module;
#include <algorithm>
#include <cmath>

module ImageProcessing;
import :Threshold;
import Core.Parallel;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

void Threshold::process(float4* buffer, int width, int height, const ThresholdSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    const float thresh = std::clamp(settings.threshold, 0.0f, 1.0f);
    const float soft = std::max(0.001f, settings.softness);
    Parallel::For(0, height, width * height, [&](int y) {
        const size_t rowStart = static_cast<size_t>(y) * static_cast<size_t>(width);
        for (int x = 0; x < width; ++x) {
            const size_t i = rowStart + static_cast<size_t>(x);
            const float4 orig = buffer[i];
            const float luma = orig.x * 0.299f + orig.y * 0.587f + orig.z * 0.114f;
            const float t = std::clamp((luma - thresh + soft * 0.5f) / soft, 0.0f, 1.0f);
            buffer[i] = { t, t, t, orig.w };
        }
    });
}

void Threshold::process(ImageF32x4_RGBA& image, const ThresholdSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()), image.width(), image.height(), settings);
}

}
