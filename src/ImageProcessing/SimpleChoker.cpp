module;
#include <algorithm>
#include <cmath>

module ImageProcessing;
import :SimpleChoker;
import Core.Parallel;

namespace ArtifactCore {

void SimpleChoker::process(float4* buffer, int width, int height, const SimpleChokerSettings& s) {
    auto* tmp = new float4[width * height];
    std::copy_n(buffer, width * height, tmp);
    int r = std::max(1, s.radius);
    Parallel::ForTiles(width, height, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float minA = 1.0f, maxA = 0.0f;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    int cx = std::clamp(x + dx, 0, width - 1);
                    int cy = std::clamp(y + dy, 0, height - 1);
                    float a = tmp[cy * width + cx].w;
                    minA = std::min(minA, a);
                    maxA = std::max(maxA, a);
                }
            }
            auto& dst = buffer[y * width + x];
            if (s.choke >= 0.0f) {
                float t = s.choke;
                dst.w = tmp[y * width + x].w * (1.0f - t) + minA * t;
            } else {
                float t = -s.choke;
                dst.w = tmp[y * width + x].w * (1.0f - t) + maxA * t;
            }
        }
        }
    });
    delete[] tmp;
}

void SimpleChoker::process(ImageF32x4_RGBA& image, const SimpleChokerSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
