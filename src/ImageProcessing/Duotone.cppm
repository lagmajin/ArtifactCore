module;
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing;
import :Duotone;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

void Duotone::process(float4* buffer, int width, int height, const DuotoneSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    size_t total_pixels = static_cast<size_t>(width * height);
    float blend = std::clamp(settings.blend, 0.0f, 1.0f);
    float4 shadow = settings.shadowColor;
    float4 highlight = settings.highlightColor;

    for (size_t i = 0; i < total_pixels; ++i) {
        float4 orig = buffer[i];

        // Perceptual luminance calculation
        float luminance = std::clamp(orig.x * 0.299f + orig.y * 0.587f + orig.z * 0.114f, 0.0f, 1.0f);

        // Linear interpolation shadow -> highlight
        float r_map = shadow.x * (1.0f - luminance) + highlight.x * luminance;
        float g_map = shadow.y * (1.0f - luminance) + highlight.y * luminance;
        float b_map = shadow.z * (1.0f - luminance) + highlight.z * luminance;

        // Blend with original pixel
        buffer[i].x = orig.x * (1.0f - blend) + r_map * blend;
        buffer[i].y = orig.y * (1.0f - blend) + g_map * blend;
        buffer[i].z = orig.z * (1.0f - blend) + b_map * blend;
        // Keep original alpha
    }
}

void Duotone::process(ImageF32x4_RGBA& image, const DuotoneSettings& settings) {
    if (image.isEmpty()) return;
    float* raw_data = image.rgba32fData();
    if (!raw_data) return;

    process(reinterpret_cast<float4*>(raw_data), image.width(), image.height(), settings);
}

} // namespace ArtifactCore
