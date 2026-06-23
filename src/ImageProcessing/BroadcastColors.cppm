module;
#include <algorithm>
#include <cmath>

module ImageProcessing:BroadcastColors;

namespace ArtifactCore {

void BroadcastColors::process(float4* buffer, int width, int height, const BroadcastColorsSettings& s) {
    float yLo = 16.0f / 255.0f, yHi = 235.0f / 255.0f;
    float uvLimit = 0.5f;
    float sat = 1.0f - s.reduceSaturation;
    for (int i = 0; i < width * height; ++i) {
        auto& px = buffer[i];
        float y = px.x * 0.299f + px.y * 0.587f + px.z * 0.114f;
        float u = px.x * -0.147f + px.y * -0.289f + px.z * 0.436f;
        float v = px.x * 0.615f + px.y * -0.515f + px.z * -0.100f;
        y = std::clamp(y, yLo, yHi);
        u = std::clamp(u, -uvLimit, uvLimit) * sat;
        v = std::clamp(v, -uvLimit, uvLimit) * sat;
        px.x = std::clamp(y + 0.000f * u + 1.140f * v, 0.0f, 1.0f);
        px.y = std::clamp(y - 0.394f * u - 0.581f * v, 0.0f, 1.0f);
        px.z = std::clamp(y + 2.032f * u + 0.000f * v, 0.0f, 1.0f);
    }
}

void BroadcastColors::process(ImageF32x4_RGBA& image, const BroadcastColorsSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
