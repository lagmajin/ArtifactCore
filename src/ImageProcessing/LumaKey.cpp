module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

module ImageProcessing;
import :LumaKey;
import Core.Parallel;
import Graphics.SurfaceColorContract;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore::Keying {
namespace {

float lumaFinite(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

} // namespace

bool processLumaKey(const LumaKeyBuffers& buffers, const LumaKeyParams& inputParams) {
    if (!buffers.src || !buffers.dst || buffers.src == buffers.dst ||
        buffers.width <= 0 || buffers.height <= 0)
        return false;
    if (buffers.channelOrder != SurfaceChannelOrder::RGBA &&
        buffers.channelOrder != SurfaceChannelOrder::BGRA)
        return false;
    const int rIndex = buffers.channelOrder == SurfaceChannelOrder::RGBA ? 0 : 2;
    constexpr int gIndex = 1;
    const int bIndex = buffers.channelOrder == SurfaceChannelOrder::RGBA ? 2 : 0;
    const auto width = static_cast<std::size_t>(buffers.width);
    const auto height = static_cast<std::size_t>(buffers.height);
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height)
        return false;
    const std::size_t pixelCount = width * height;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4)
        return false;
    const int w = buffers.width;
    const int h = buffers.height;

    LumaKeyParams p = inputParams;
    p.low = std::clamp(lumaFinite(p.low, 0.15f), 0.0f, 1.0f);
    p.high = std::clamp(lumaFinite(p.high, 0.85f), 0.0f, 1.0f);
    if (p.high < p.low) std::swap(p.low, p.high);
    p.softness = std::max(0.001f, lumaFinite(p.softness, 0.08f));
    p.choke = std::clamp(lumaFinite(p.choke, 0.0f), -1.0f, 1.0f);
    p.matteBlur = std::clamp(lumaFinite(p.matteBlur, 0.0f), 0.0f, 2.0f);
    p.viewMode = std::clamp(p.viewMode, 0, 1);

    std::vector<float> matte(pixelCount);
    Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * width +
                    static_cast<std::size_t>(x);
                const float* s = buffers.src + i * 4;
                const float luma = s[rIndex] * 0.2126f + s[gIndex] * 0.7152f +
                                   s[bIndex] * 0.0722f;
                if (!std::isfinite(luma) || !std::isfinite(s[3])) {
                    matte[i] = 0.0f;
                    continue;
                }
                const float lower = std::clamp((luma - p.low) / p.softness, 0.0f, 1.0f);
                const float upper = std::clamp((p.high - luma) / p.softness, 0.0f, 1.0f);
                matte[i] = std::min(lower, upper);
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
                    d[rIndex] = d[gIndex] = d[bIndex] = alpha;
                    d[3] = 1.0f;
                } else {
                    d[rIndex] = s[rIndex];
                    d[gIndex] = s[gIndex];
                    d[bIndex] = s[bIndex];
                    d[3] = std::clamp(s[3] * alpha, 0.0f, 1.0f);
                }
            }
        }
    });
    return true;
}

} // namespace ArtifactCore::Keying

namespace ArtifactCore {

void LumaKey::process(float4* buffer, int width, int height, const LumaKeySettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    const float lo = settings.lowThreshold;
    const float hi = std::max(lo, settings.highThreshold);
    const float soft = std::max(0.001f, settings.softness);
    const float softInv = 1.0f / soft;
    Parallel::ForTiles(width, height, 64, 64, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            const size_t rowStart = static_cast<size_t>(y) * static_cast<size_t>(width);
            for (int x = x0; x < x1; ++x) {
                const size_t i = rowStart + static_cast<size_t>(x);
                float4 c = buffer[i];
            const float luma = c.z * 0.299f + c.y * 0.587f + c.x * 0.114f;

            float alpha = 1.0f;
            if (luma < lo)
                alpha = std::clamp((luma - (lo - soft)) * softInv, 0.0f, 1.0f);
            else if (luma > hi)
                alpha = std::clamp(((hi + soft) - luma) * softInv, 0.0f, 1.0f);

                buffer[i] = { c.x, c.y, c.z, c.w * alpha };
            }
        }
    });
}

void LumaKey::process(ImageF32x4_RGBA& image, const LumaKeySettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()), image.width(), image.height(), settings);
}

}
