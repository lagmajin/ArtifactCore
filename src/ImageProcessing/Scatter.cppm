module;
#include <algorithm>
#include <random>
#include <vector>

module ImageProcessing;
import :Scatter;
import Core.Parallel;

namespace ArtifactCore {

void Scatter::process(float4* buffer, int width, int height, const ScatterSettings& s) {
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<float4> tmp(pixelCount);
    std::copy_n(buffer, pixelCount, tmp.data());
    std::vector<size_t> sourceIndices(pixelCount);
    std::mt19937 rng(static_cast<unsigned>(s.seed));
    float inv = 1.0f / 65535.0f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float ox = (static_cast<float>(rng() & 0xFFFF) * inv * 2.0f - 1.0f) * s.amount;
            float oy = (static_cast<float>(rng() & 0xFFFF) * inv * 2.0f - 1.0f) * s.amount;
            int sx = std::clamp(static_cast<int>(x + ox), 0, width - 1);
            int sy = std::clamp(static_cast<int>(y + oy), 0, height - 1);
            sourceIndices[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                static_cast<size_t>(sy) * static_cast<size_t>(width) + static_cast<size_t>(sx);
        }
    }
    Parallel::ForTiles(width, height, 64, 64, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            buffer[index] = tmp[sourceIndices[index]];
        }
        }
    });
}

void Scatter::process(ImageF32x4_RGBA& image, const ScatterSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
