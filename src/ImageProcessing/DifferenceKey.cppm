module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

module ImageProcessing;
import :DifferenceKey;
import Core.Parallel;

namespace ArtifactCore::Keying {
namespace {

float diffFinite(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

} // namespace

bool processDifferenceKey(const DifferenceKeyBuffers& buffers,
                          const DifferenceKeyParams& inputParams) {
    if (!buffers.src || !buffers.dst || buffers.src == buffers.dst ||
        buffers.width <= 0 || buffers.height <= 0)
        return false;
    const auto width = static_cast<std::size_t>(buffers.width);
    const auto height = static_cast<std::size_t>(buffers.height);
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height)
        return false;
    const std::size_t pixelCount = width * height;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4)
        return false;
    const int w = buffers.width;
    const int h = buffers.height;

    DifferenceKeyParams p = inputParams;
    p.refR = std::clamp(diffFinite(p.refR, 0.0f), 0.0f, 1.0f);
    p.refG = std::clamp(diffFinite(p.refG, 0.0f), 0.0f, 1.0f);
    p.refB = std::clamp(diffFinite(p.refB, 0.0f), 0.0f, 1.0f);
    p.threshold = std::clamp(diffFinite(p.threshold, 0.1f), 0.0f, 1.732f);
    p.softness = std::max(0.001f, diffFinite(p.softness, 0.08f));
    p.choke = std::clamp(diffFinite(p.choke, 0.0f), -1.0f, 1.0f);
    p.matteBlur = std::clamp(diffFinite(p.matteBlur, 0.0f), 0.0f, 2.0f);
    p.viewMode = std::clamp(p.viewMode, 0, 1);

    std::vector<float> matte(pixelCount);
    Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * width +
                    static_cast<std::size_t>(x);
                const float* s = buffers.src + i * 4;
                const float dr = s[0] - p.refR;
                const float dg = s[1] - p.refG;
                const float db = s[2] - p.refB;
                const float distance = std::sqrt(dr * dr + dg * dg + db * db);
                if (!std::isfinite(distance) || !std::isfinite(s[3])) {
                    matte[i] = 0.0f;
                    continue;
                }
                matte[i] = std::clamp((distance - p.threshold) / p.softness,
                                      0.0f, 1.0f);
            }
        }
    });

    const bool needChoke = std::abs(p.choke) > 1.0e-4f;
    const bool needBlur = p.matteBlur > 1.0e-4f;
    if (needChoke) {
        const int radius = std::clamp(static_cast<int>(std::abs(p.choke) * 3.0f + 0.5f), 1, 3);
        const bool erode = p.choke > 0.0f;
        std::vector<float> result(pixelCount);
        Parallel::ForTiles(w, h, 16, 16, [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    float value = erode ? 1.0f : 0.0f;
                    for (int dy = -radius; dy <= radius; ++dy) {
                        for (int dx = -radius; dx <= radius; ++dx) {
                            const int sx = std::clamp(x + dx, 0, w - 1);
                            const int sy = std::clamp(y + dy, 0, h - 1);
                            const float sample = matte[static_cast<std::size_t>(sy) * width +
                                                       static_cast<std::size_t>(sx)];
                            value = erode ? std::min(value, sample) : std::max(value, sample);
                        }
                    }
                    result[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = value;
                }
            }
        });
        matte.swap(result);
    }
    if (needBlur) {
        const float t = std::clamp(p.matteBlur * 0.5f, 0.0f, 1.0f);
        std::vector<float> result(pixelCount);
        Parallel::ForTiles(w, h, 16, 16, [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    float sum = 0.0f;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int sx = std::clamp(x + dx, 0, w - 1);
                            const int sy = std::clamp(y + dy, 0, h - 1);
                            sum += matte[static_cast<std::size_t>(sy) * width +
                                         static_cast<std::size_t>(sx)];
                        }
                    }
                    const float base = matte[static_cast<std::size_t>(y) * width +
                                             static_cast<std::size_t>(x)];
                    result[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] =
                        base + (sum / 9.0f - base) * t;
                }
            }
        });
        matte.swap(result);
    }

    Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * width +
                    static_cast<std::size_t>(x);
                const float* s = buffers.src + i * 4;
                float* d = buffers.dst + i * 4;
                const float alpha = std::clamp(matte[i], 0.0f, 1.0f);
                if (p.viewMode == 1) {
                    d[0] = d[1] = d[2] = alpha;
                    d[3] = 1.0f;
                } else {
                    d[0] = s[0];
                    d[1] = s[1];
                    d[2] = s[2];
                    d[3] = std::clamp(s[3] * alpha, 0.0f, 1.0f);
                }
            }
        }
    });
    return true;
}

} // namespace ArtifactCore::Keying
