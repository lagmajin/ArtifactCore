module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

module ImageProcessing;
import :ChromaKey;
import Core.Parallel;

namespace ArtifactCore::Keying {
namespace {

float finiteOr(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}

float smoothstep(float edge0, float edge1, float value) {
    if (edge1 <= edge0) return value > edge0 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct YCbCr {
    float y = 0.0f;
    float cb = 0.0f;
    float cr = 0.0f;
};

YCbCr rgbToYCbCr(float r, float g, float b) {
    const float y = r * 0.299f + g * 0.587f + b * 0.114f;
    return YCbCr{y, (b - y) * 0.564f, (r - y) * 0.713f};
}

float baseAlphaFor(float r, float g, float b, const YCbCr& key,
                   float hueTol, float softness, float clipBlack, float clipWhite) {
    const YCbCr pix = rgbToYCbCr(r, g, b);
    const float dCb = pix.cb - key.cb;
    const float dCr = pix.cr - key.cr;
    const float chromaDist = std::sqrt(dCb * dCb + dCr * dCr);
    const float lumaDist = std::abs(pix.y - key.y);
    const float combined = std::sqrt(chromaDist * chromaDist + (lumaDist * 0.35f) * (lumaDist * 0.35f));
    const float soft = std::max(1.0e-4f, softness);
    float alpha = smoothstep(hueTol, hueTol + soft, combined);
    const float range = std::max(1.0e-4f, clipWhite - clipBlack);
    alpha = std::clamp((alpha - clipBlack) / range, 0.0f, 1.0f);
    return alpha;
}

void despillPixel(float& r, float& g, float& b, float kr, float kg, float kb,
                  float alpha, float strength, int mode) {
    if (mode <= 0 || strength <= 0.0f || alpha >= 0.999f) return;
    const float spill = (1.0f - alpha) * std::clamp(strength, 0.0f, 1.0f);
    if (spill <= 0.0f) return;
    if (mode == 1) {
        // Suppress only the key-dominant channel excess over the other two.
        if (kg >= kr && kg >= kb) {
            const float excess = std::max(0.0f, g - std::max(r, b));
            g -= excess * spill;
        } else if (kr >= kg && kr >= kb) {
            const float excess = std::max(0.0f, r - std::max(g, b));
            r -= excess * spill;
        } else {
            const float excess = std::max(0.0f, b - std::max(r, g));
            b -= excess * spill;
        }
    } else {
        // Luminance-preserving desaturation toward luma.
        const float y = r * 0.299f + g * 0.587f + b * 0.114f;
        r += (y - r) * spill;
        g += (y - g) * spill;
        b += (y - b) * spill;
    }
    r = std::max(0.0f, r);
    g = std::max(0.0f, g);
    b = std::max(0.0f, b);
}

} // namespace

ChromaKeyGpuParams ChromaKeyGpuParams::fromParams(const ChromaKeyParams& source) {
    ChromaKeyGpuParams result;
    result.keyR = std::clamp(finiteOr(source.keyColor.x, 0.0f), 0.0f, 1.0f);
    result.keyG = std::clamp(finiteOr(source.keyColor.y, 1.0f), 0.0f, 1.0f);
    result.keyB = std::clamp(finiteOr(source.keyColor.z, 0.0f), 0.0f, 1.0f);
    result.pad0 = 0.0f;
    result.hueTolerance = std::clamp(finiteOr(source.hueTolerance, 0.28f), 0.0f, 1.0f);
    result.edgeSoftness = std::max(1.0e-4f, finiteOr(source.edgeSoftness, 0.12f));
    result.clipBlack = std::clamp(finiteOr(source.clipBlack, 0.0f), 0.0f, 1.0f);
    result.clipWhite = std::clamp(finiteOr(source.clipWhite, 1.0f), 0.0f, 1.0f);
    if (result.clipWhite < result.clipBlack + 1.0e-4f)
        result.clipWhite = std::min(1.0f, result.clipBlack + 1.0e-4f);
    result.despillStrength = std::clamp(finiteOr(source.despillStrength, 0.7f), 0.0f, 1.0f);
    result.despillMode = std::clamp(source.despillMode, 0, 2);
    result.viewMode = std::clamp(source.viewMode, 0, 3);
    result.choke = std::clamp(finiteOr(source.choke, 0.0f), -1.0f, 1.0f);
    result.matteBlur = std::clamp(finiteOr(source.matteBlur, 0.0f), 0.0f, 2.0f);
    result.pad1 = result.pad2 = result.pad3 = 0.0f;
    return result;
}

bool processChromaKey(const ChromaKeyBuffers& buffers, const ChromaKeyParams& inputParams) {
    if (!buffers.src || !buffers.dst || buffers.width <= 0 || buffers.height <= 0)
        return false;
    const auto width = static_cast<std::size_t>(buffers.width);
    const auto height = static_cast<std::size_t>(buffers.height);
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height)
        return false;
    const std::size_t pixelCount = width * height;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4)
        return false;
    if (buffers.src == buffers.dst)
        return false;

    ChromaKeyParams p = inputParams;
    p.keyColor.x = std::clamp(finiteOr(p.keyColor.x, 0.0f), 0.0f, 1.0f);
    p.keyColor.y = std::clamp(finiteOr(p.keyColor.y, 1.0f), 0.0f, 1.0f);
    p.keyColor.z = std::clamp(finiteOr(p.keyColor.z, 0.0f), 0.0f, 1.0f);
    p.hueTolerance = std::clamp(finiteOr(p.hueTolerance, 0.28f), 0.0f, 1.0f);
    p.edgeSoftness = std::max(1.0e-4f, finiteOr(p.edgeSoftness, 0.12f));
    p.clipBlack = std::clamp(finiteOr(p.clipBlack, 0.0f), 0.0f, 1.0f);
    p.clipWhite = std::clamp(finiteOr(p.clipWhite, 1.0f), 0.0f, 1.0f);
    if (p.clipWhite < p.clipBlack + 1.0e-4f)
        p.clipWhite = std::min(1.0f, p.clipBlack + 1.0e-4f);
    p.despillStrength = std::clamp(finiteOr(p.despillStrength, 0.7f), 0.0f, 1.0f);
    p.despillMode = std::clamp(p.despillMode, 0, 2);
    p.choke = std::clamp(finiteOr(p.choke, 0.0f), -1.0f, 1.0f);
    p.matteBlur = std::clamp(finiteOr(p.matteBlur, 0.0f), 0.0f, 2.0f);
    p.viewMode = std::clamp(p.viewMode, 0, 3);

    const YCbCr key = rgbToYCbCr(p.keyColor.x, p.keyColor.y, p.keyColor.z);
    const int w = buffers.width;
    const int h = buffers.height;

    // Pass 1: per-pixel screen matte + despill (no cross-pixel reads).
    Parallel::ForTiles(w, h, 32, 32, [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
                const float* s = buffers.src + i * 4;
                float* d = buffers.dst + i * 4;
                float r = s[0], g = s[1], b = s[2];
                const float srcA = std::clamp(std::isfinite(s[3]) ? s[3] : 0.0f, 0.0f, 1.0f);
                if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
                    d[0] = d[1] = d[2] = d[3] = 0.0f;
                    continue;
                }
                float alpha = baseAlphaFor(r, g, b, key, p.hueTolerance,
                                           p.edgeSoftness, p.clipBlack, p.clipWhite);
                despillPixel(r, g, b, p.keyColor.x, p.keyColor.y, p.keyColor.z,
                             alpha, p.despillStrength, p.despillMode);
                const float outA = std::clamp(srcA * alpha, 0.0f, 1.0f);
                if (p.viewMode == 1) {
                    d[0] = d[1] = d[2] = alpha;
                    d[3] = 1.0f;
                } else if (p.viewMode == 2) {
                    d[0] = r;
                    d[1] = g;
                    d[2] = b;
                    d[3] = 1.0f;
                } else if (p.viewMode == 3) {
                    // Status: red=culled, green=opaque, blend in between.
                    d[0] = 1.0f - alpha;
                    d[1] = alpha;
                    d[2] = 0.15f * (1.0f - std::abs(alpha * 2.0f - 1.0f));
                    d[3] = 1.0f;
                } else {
                    d[0] = r;
                    d[1] = g;
                    d[2] = b;
                    d[3] = outA;
                }
            }
        }
    });

    if (p.viewMode != 0)
        return true;

    // Pass 2: edge finishing on the alpha plane only. Bounded temp plane
    // (W*H floats); skipped entirely when choke/blur are zero. Same
    // exception rationale as IBK's per-frame matte buffer.
    const bool needChoke = std::abs(p.choke) > 1.0e-4f;
    const bool needBlur = p.matteBlur > 1.0e-4f;
    if (!needChoke && !needBlur)
        return true;

    std::vector<float> matte(pixelCount);
    for (std::size_t i = 0; i < pixelCount; ++i)
        matte[i] = buffers.dst[i * 4 + 3];

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
                            const float sample = matte[static_cast<std::size_t>(sy) * width + static_cast<std::size_t>(sx)];
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
                            sum += matte[static_cast<std::size_t>(sy) * width + static_cast<std::size_t>(sx)];
                        }
                    }
                    const float mean = sum / 9.0f;
                    const float base = matte[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
                    result[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = base + (mean - base) * t;
                }
            }
        });
        matte.swap(result);
    }

    for (std::size_t i = 0; i < pixelCount; ++i)
        buffers.dst[i * 4 + 3] = std::clamp(matte[i], 0.0f, 1.0f);
    return true;
}

} // namespace ArtifactCore::Keying

namespace ArtifactCore {

static float colorDist(const float3& a, const float3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz) * 0.57735f;
}

void ChromaKey::process(float4* buffer, int width, int height, const ChromaKeySettings& s) {
    if (!buffer || width <= 0 || height <= 0) return;
    // Deprecated path: forward to the Keylight-style keyer with an
    // approximate tolerance mapping so legacy callers keep working.
    Keying::ChromaKeyParams params;
    params.keyColor = float4{s.keyColor.x, s.keyColor.y, s.keyColor.z, 1.0f};
    params.hueTolerance = std::clamp(s.tolerance * 0.9f, 0.0f, 1.0f);
    params.edgeSoftness = std::max(1.0e-4f, s.softness);
    params.despillMode = 2;
    Keying::ChromaKeyBuffers buffers{reinterpret_cast<const float*>(buffer),
                                     reinterpret_cast<float*>(buffer), width, height};
    // In-place forward needs a snapshot because the new keyer requires
    // distinct src/dst (edge finishing reads neighbours).
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    std::vector<float> snapshot(count);
    std::copy_n(reinterpret_cast<const float*>(buffer), count, snapshot.data());
    buffers.src = snapshot.data();
    Keying::processChromaKey(buffers, params);
}

void ChromaKey::process(ImageF32x4_RGBA& image, const ChromaKeySettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
