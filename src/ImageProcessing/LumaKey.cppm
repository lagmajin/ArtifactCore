module;
#include <algorithm>
#include <cmath>

module ImageProcessing:LumaKey;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

void LumaKey::process(float4* buffer, int width, int height, const LumaKeySettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    const float lo = settings.lowThreshold;
    const float hi = std::max(lo, settings.highThreshold);
    const float soft = std::max(0.001f, settings.softness);
    const float softInv = 1.0f / soft;
    const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);

    for (size_t i = 0; i < total; ++i) {
        float4 c = buffer[i];
        const float luma = c.x * 0.299f + c.y * 0.587f + c.z * 0.114f;

        float alpha = 1.0f;
        if (luma < lo)
            alpha = std::clamp((luma - (lo - soft)) * softInv, 0.0f, 1.0f);
        else if (luma > hi)
            alpha = std::clamp(((hi + soft) - luma) * softInv, 0.0f, 1.0f);

        buffer[i] = { c.x, c.y, c.z, c.w * alpha };
    }
}

void LumaKey::process(ImageF32x4_RGBA& image, const LumaKeySettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()), image.width(), image.height(), settings);
}

}
