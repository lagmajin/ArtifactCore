module;
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing:Median;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

namespace {

void sort3(float v[9]) {
    auto& a = v;
#define SWAP(i, j) { if (a[i] > a[j]) { float t = a[i]; a[i] = a[j]; a[j] = t; } }
    SWAP(0, 1); SWAP(2, 3); SWAP(4, 5); SWAP(6, 7);
    SWAP(0, 2); SWAP(1, 3); SWAP(4, 6); SWAP(5, 7);
    SWAP(1, 2); SWAP(5, 6); SWAP(0, 4); SWAP(3, 7);
    SWAP(1, 5); SWAP(2, 6); SWAP(1, 4); SWAP(3, 5);
    SWAP(2, 4); SWAP(3, 4);
#undef SWAP
}

void median3x3(const float4* src, float4* dst, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r[9], g[9], b[9], a[9];
            int idx = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int sx = std::clamp(x + dx, 0, w - 1);
                    int sy = std::clamp(y + dy, 0, h - 1);
                    float4 c = src[static_cast<size_t>(sy) * w + static_cast<size_t>(sx)];
                    r[idx] = c.x; g[idx] = c.y; b[idx] = c.z; a[idx] = c.w;
                    ++idx;
                }
            }
            sort3(r); sort3(g); sort3(b); sort3(a);
            dst[static_cast<size_t>(y) * w + static_cast<size_t>(x)] = { r[4], g[4], b[4], a[4] };
        }
    }
}

} // anonymous namespace

void Median::process(float4* buffer, int width, int height, const MedianSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;
    const int r = std::max(1, settings.radius);

    if (r == 1) {
        std::vector<float4> tmp(static_cast<size_t>(width) * static_cast<size_t>(height));
        median3x3(buffer, tmp.data(), width, height);
        std::copy(tmp.begin(), tmp.end(), buffer);
    }
}

void Median::process(ImageF32x4_RGBA& image, const MedianSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()), image.width(), image.height(), settings);
}

}
