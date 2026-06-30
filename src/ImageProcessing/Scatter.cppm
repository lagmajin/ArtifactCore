module;
#include <algorithm>
#include <random>

module ImageProcessing;
import :Scatter;

namespace ArtifactCore {

void Scatter::process(float4* buffer, int width, int height, const ScatterSettings& s) {
    auto* tmp = new float4[width * height];
    std::copy_n(buffer, width * height, tmp);
    std::mt19937 rng(static_cast<unsigned>(s.seed));
    float inv = 1.0f / 65535.0f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float ox = (static_cast<float>(rng() & 0xFFFF) * inv * 2.0f - 1.0f) * s.amount;
            float oy = (static_cast<float>(rng() & 0xFFFF) * inv * 2.0f - 1.0f) * s.amount;
            int sx = std::clamp(static_cast<int>(x + ox), 0, width - 1);
            int sy = std::clamp(static_cast<int>(y + oy), 0, height - 1);
            buffer[y * width + x] = tmp[sy * width + sx];
        }
    }
    delete[] tmp;
}

void Scatter::process(ImageF32x4_RGBA& image, const ScatterSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
