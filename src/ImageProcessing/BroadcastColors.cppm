module;
#include <algorithm>
#include <cmath>

module ImageProcessing;
import :BroadcastColors;
import Core.Parallel;

namespace ArtifactCore {

void BroadcastColors::process(float4* buffer, int width, int height, const BroadcastColorsSettings& s) {
    float yLo = 16.0f / 255.0f, yHi = 235.0f / 255.0f;
    float uvLimit = 0.5f;
    float sat = 1.0f - s.reduceSaturation;
    Parallel::ForTiles(width, height, 32, 32, [&](int x0, int y0, int x1, int y1) {
    for (int row = y0; row < y1; ++row) {
        const int rowBase = row * width;
        for (int x = x0; x < x1; ++x) {
            auto& px = buffer[rowBase + x];
            const float r = px.z;
            const float g = px.y;
            const float b = px.x;
            float y = r * 0.299f + g * 0.587f + b * 0.114f;
            float u = r * -0.147f + g * -0.289f + b * 0.436f;
            float v = r * 0.615f + g * -0.515f + b * -0.100f;
            y = std::clamp(y, yLo, yHi);
            u = std::clamp(u, -uvLimit, uvLimit) * sat;
            v = std::clamp(v, -uvLimit, uvLimit) * sat;
            const float outR = std::clamp(y + 0.000f * u + 1.140f * v, 0.0f, 1.0f);
            px.y = std::clamp(y - 0.394f * u - 0.581f * v, 0.0f, 1.0f);
            const float outB = std::clamp(y + 2.032f * u + 0.000f * v, 0.0f, 1.0f);
            px.x = outB;
            px.z = outR;
        }
    }
    });
}

void BroadcastColors::process(ImageF32x4_RGBA& image, const BroadcastColorsSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
