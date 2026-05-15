module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <QDebug>
#include <QString>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ImageProcessing.ProceduralTexture;

import Graphics.Compute;
import Graphics.Shader.Compute.HLSL.ProceduralTexture;

namespace ArtifactCore
{
using namespace Diligent;

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

struct Float2
{
    float x = 0.0f;
    float y = 0.0f;
};

static inline Float2 makeFloat2(float x, float y)
{
    return {x, y};
}

static inline Float2 add(const Float2& a, const Float2& b)
{
    return {a.x + b.x, a.y + b.y};
}

static inline Float2 sub(const Float2& a, const Float2& b)
{
    return {a.x - b.x, a.y - b.y};
}

static inline Float2 mul(const Float2& a, float s)
{
    return {a.x * s, a.y * s};
}

static inline Float2 mul(const Float2& a, const Float2& b)
{
    return {a.x * b.x, a.y * b.y};
}

static inline float dot(const Float2& a, const Float2& b)
{
    return a.x * b.x + a.y * b.y;
}

static inline float length(const Float2& a)
{
    return std::sqrt(dot(a, a));
}

static inline Float2 rotate(const Float2& v, float angle)
{
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    return {c * v.x - s * v.y, s * v.x + c * v.y};
}

static inline float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

static inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline float fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline std::uint32_t hashU32(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static inline std::uint32_t hash2D(int x, int y, std::uint32_t seed)
{
    return hashU32(static_cast<std::uint32_t>(x) ^ (static_cast<std::uint32_t>(y) * 0x9e3779b9u) ^ (seed * 0x85ebca6bu));
}

static inline float hash01(std::uint32_t h)
{
    return static_cast<float>(h & 0x00ffffffu) / 16777215.0f;
}

static inline int positiveMod(int value, int period)
{
    if (period <= 0)
    {
        return 0;
    }
    int r = value % period;
    return r < 0 ? r + period : r;
}

static inline int periodFromScale(float scale)
{
    const float absScale = std::max(1.0f, std::abs(scale));
    return std::max(1, static_cast<int>(std::round(absScale)));
}

static inline Float2 gradientFromHash(std::uint32_t h, float rotation)
{
    const float a = hash01(h) * kTwoPi + rotation;
    return {std::cos(a), std::sin(a)};
}

static float sampleValueNoise(const Float2& p, int periodX, int periodY, std::uint32_t seed)
{
    const int ix = static_cast<int>(std::floor(p.x));
    const int iy = static_cast<int>(std::floor(p.y));
    const float fx = p.x - static_cast<float>(ix);
    const float fy = p.y - static_cast<float>(iy);

    const int x0 = positiveMod(ix, periodX);
    const int y0 = positiveMod(iy, periodY);
    const int x1 = positiveMod(ix + 1, periodX);
    const int y1 = positiveMod(iy + 1, periodY);

    const float v00 = hash01(hash2D(x0, y0, seed));
    const float v10 = hash01(hash2D(x1, y0, seed));
    const float v01 = hash01(hash2D(x0, y1, seed));
    const float v11 = hash01(hash2D(x1, y1, seed));

    const float u = fade(fx);
    const float v = fade(fy);
    return lerp(lerp(v00, v10, u), lerp(v01, v11, u), v);
}

static float samplePerlinNoise(const Float2& p, int periodX, int periodY, std::uint32_t seed, float rotation)
{
    const int ix = static_cast<int>(std::floor(p.x));
    const int iy = static_cast<int>(std::floor(p.y));
    const float fx = p.x - static_cast<float>(ix);
    const float fy = p.y - static_cast<float>(iy);
    const float u = fade(fx);
    const float v = fade(fy);

    const int x0 = positiveMod(ix, periodX);
    const int y0 = positiveMod(iy, periodY);
    const int x1 = positiveMod(ix + 1, periodX);
    const int y1 = positiveMod(iy + 1, periodY);

    const Float2 g00 = gradientFromHash(hash2D(x0, y0, seed), rotation);
    const Float2 g10 = gradientFromHash(hash2D(x1, y0, seed), rotation);
    const Float2 g01 = gradientFromHash(hash2D(x0, y1, seed), rotation);
    const Float2 g11 = gradientFromHash(hash2D(x1, y1, seed), rotation);

    const float n00 = dot(g00, {fx, fy});
    const float n10 = dot(g10, {fx - 1.0f, fy});
    const float n01 = dot(g01, {fx, fy - 1.0f});
    const float n11 = dot(g11, {fx - 1.0f, fy - 1.0f});

    const float xBlend0 = lerp(n00, n10, u);
    const float xBlend1 = lerp(n01, n11, u);
    return clamp01(0.5f + 0.5f * lerp(xBlend0, xBlend1, v));
}

static float sampleSimplexNoise(const Float2& p, int periodX, int periodY, std::uint32_t seed, float rotation)
{
    constexpr float F2 = 0.3660254037844386f;
    constexpr float G2 = 0.2113248654051871f;

    const float s = (p.x + p.y) * F2;
    const float sx = p.x + s;
    const float sy = p.y + s;
    const int i = static_cast<int>(std::floor(sx));
    const int j = static_cast<int>(std::floor(sy));
    const float t = static_cast<float>(i + j) * G2;
    const float X0 = static_cast<float>(i) - t;
    const float Y0 = static_cast<float>(j) - t;
    const Float2 x0 = {p.x - X0, p.y - Y0};

    const Float2 x1 = (x0.x > x0.y)
        ? Float2{x0.x - 1.0f + G2, x0.y + G2}
        : Float2{x0.x + G2, x0.y - 1.0f + G2};
    const Float2 x2 = {x0.x - 1.0f + 2.0f * G2, x0.y - 1.0f + 2.0f * G2};

    const int i1x = x0.x > x0.y ? 1 : 0;
    const int i1y = x0.x > x0.y ? 0 : 1;

    const int ii0 = positiveMod(i, periodX);
    const int jj0 = positiveMod(j, periodY);
    const int ii1 = positiveMod(i + i1x, periodX);
    const int jj1 = positiveMod(j + i1y, periodY);
    const int ii2 = positiveMod(i + 1, periodX);
    const int jj2 = positiveMod(j + 1, periodY);

    const Float2 g0 = gradientFromHash(hash2D(ii0, jj0, seed), rotation);
    const Float2 g1 = gradientFromHash(hash2D(ii1, jj1, seed), rotation);
    const Float2 g2 = gradientFromHash(hash2D(ii2, jj2, seed), rotation);

    const float t0 = 0.5f - dot(x0, x0);
    const float t1 = 0.5f - dot(x1, x1);
    const float t2 = 0.5f - dot(x2, x2);

    float n0 = 0.0f;
    float n1 = 0.0f;
    float n2 = 0.0f;
    if (t0 > 0.0f) n0 = t0 * t0 * t0 * t0 * dot(g0, x0);
    if (t1 > 0.0f) n1 = t1 * t1 * t1 * t1 * dot(g1, x1);
    if (t2 > 0.0f) n2 = t2 * t2 * t2 * t2 * dot(g2, x2);

    return clamp01(0.5f + 0.5f * (70.0f * (n0 + n1 + n2)));
}

static float sampleWhiteNoise(const Float2& p, int periodX, int periodY, std::uint32_t seed)
{
    const int ix = positiveMod(static_cast<int>(std::floor(p.x)), periodX);
    const int iy = positiveMod(static_cast<int>(std::floor(p.y)), periodY);
    return hash01(hash2D(ix, iy, seed));
}

static float sampleVoronoiNoise(const Float2& p,
                                int periodX,
                                int periodY,
                                std::uint32_t seed,
                                ProceduralTextureVoronoiMode mode,
                                float jitter,
                                float rotation)
{
    const int baseX = static_cast<int>(std::floor(p.x));
    const int baseY = static_cast<int>(std::floor(p.y));
    float closest = std::numeric_limits<float>::max();
    float secondClosest = std::numeric_limits<float>::max();
    float cellValue = 0.0f;

    for (int oy = -1; oy <= 1; ++oy)
    {
        for (int ox = -1; ox <= 1; ++ox)
        {
            const int cellX = positiveMod(baseX + ox, periodX);
            const int cellY = positiveMod(baseY + oy, periodY);
            const std::uint32_t h = hash2D(cellX, cellY, seed);
            Float2 offset = {
                hash01(hashU32(h ^ 0x68bc21ebu)),
                hash01(hashU32(h ^ 0x02e5be93u))
            };
            offset = rotate(sub(offset, {0.5f, 0.5f}), rotation);
            offset = add(mul(offset, jitter), {0.5f, 0.5f});

            const Float2 feature = {static_cast<float>(baseX + ox) + offset.x,
                                     static_cast<float>(baseY + oy) + offset.y};
            const float d = length(sub(feature, p));
            if (d < closest)
            {
                secondClosest = closest;
                closest = d;
                cellValue = hash01(h);
            }
            else if (d < secondClosest)
            {
                secondClosest = d;
            }
        }
    }

    switch (mode)
    {
    case ProceduralTextureVoronoiMode::Cell:
        return cellValue;
    case ProceduralTextureVoronoiMode::Edge:
        return clamp01((secondClosest - closest) * 4.0f);
    case ProceduralTextureVoronoiMode::Distance:
    default:
        return clamp01(1.0f - closest * 1.41421356f);
    }
}

static float sampleGradientNoise(const Float2& uv,
                                 int periodX,
                                 int periodY,
                                 const ProceduralTextureGeneratorParams& params)
{
    switch (params.gradientMode)
    {
    case ProceduralTextureGradientMode::Radial:
    {
        const Float2 center = {0.5f, 0.5f};
        Float2 delta = sub(uv, center);
        delta.x = std::abs(delta.x);
        delta.y = std::abs(delta.y);
        delta.x = std::min(delta.x, 1.0f - delta.x);
        delta.y = std::min(delta.y, 1.0f - delta.y);
        const float dist = length(mul(delta, 2.0f));
        return clamp01(1.0f - dist);
    }
    case ProceduralTextureGradientMode::Linear:
    default:
    {
        const float phase = uv.x * std::cos(params.rotation) + uv.y * std::sin(params.rotation);
        return clamp01(0.5f + 0.5f * std::sin((phase + params.offset[0]) * kTwoPi));
    }
    }
}

static float sampleBaseGenerator(const ProceduralTextureGeneratorParams& params,
                                 const Float2& p,
                                 int periodX,
                                 int periodY)
{
    switch (params.kind)
    {
    case ProceduralTextureGeneratorKind::Simplex:
        return sampleSimplexNoise(p, periodX, periodY, params.seed, params.rotation);
    case ProceduralTextureGeneratorKind::FBM:
    {
        float sum = 0.0f;
        float amp = 1.0f;
        float norm = 0.0f;
        Float2 freq = p;
        int octavePeriodX = periodX;
        int octavePeriodY = periodY;
        const std::uint32_t octaves = std::max(1u, params.octaves);
        for (std::uint32_t i = 0; i < octaves; ++i)
        {
            sum += samplePerlinNoise(freq, octavePeriodX, octavePeriodY, params.seed + i * 31u, params.rotation) * amp;
            norm += amp;
            amp *= params.gain;
            freq = mul(freq, std::max(params.lacunarity, 1.0f));
            octavePeriodX = std::max(1, static_cast<int>(std::round(static_cast<float>(octavePeriodX) * std::max(params.lacunarity, 1.0f))));
            octavePeriodY = std::max(1, static_cast<int>(std::round(static_cast<float>(octavePeriodY) * std::max(params.lacunarity, 1.0f))));
        }
        return norm > 0.0f ? clamp01(sum / norm) : 0.0f;
    }
    case ProceduralTextureGeneratorKind::Voronoi:
        return sampleVoronoiNoise(p, periodX, periodY, params.seed, params.voronoiMode, params.cellJitter, params.rotation);
    case ProceduralTextureGeneratorKind::White:
        return sampleWhiteNoise(p, periodX, periodY, params.seed);
    case ProceduralTextureGeneratorKind::Value:
        return sampleValueNoise(p, periodX, periodY, params.seed);
    case ProceduralTextureGeneratorKind::Gradient:
        return sampleGradientNoise({p.x / std::max(1.0f, static_cast<float>(periodX)),
                                    p.y / std::max(1.0f, static_cast<float>(periodY))},
                                   periodX, periodY, params);
    case ProceduralTextureGeneratorKind::Perlin:
    default:
        return samplePerlinNoise(p, periodX, periodY, params.seed, params.rotation);
    }
}

static float applyBlend(float a, float b, ProceduralTextureBlendMode mode, float weight)
{
    weight = std::clamp(weight, 0.0f, 1.0f);
    switch (mode)
    {
    case ProceduralTextureBlendMode::Multiply:
        return lerp(a, a * b, weight);
    case ProceduralTextureBlendMode::Overlay:
        return lerp(a, (a < 0.5f) ? (2.0f * a * b) : (1.0f - 2.0f * (1.0f - a) * (1.0f - b)), weight);
    case ProceduralTextureBlendMode::Add:
    default:
        return clamp01(a + b * weight);
    }
}

static float applyPost(const ProceduralTextureSettings& settings, float value)
{
    const auto& post = settings.post;
    if (post.invert)
    {
        value = 1.0f - value;
    }

    if (post.remapEnabled)
    {
        const float denom = std::max(post.remapInMax - post.remapInMin, 1e-6f);
        const float t = clamp01((value - post.remapInMin) / denom);
        value = lerp(post.remapOutMin, post.remapOutMax, t);
    }

    if (post.clampEnabled)
    {
        value = std::clamp(value, post.clampMin, post.clampMax);
    }

    value = std::pow(clamp01(value), std::max(post.gamma, 1e-6f));
    return clamp01(value);
}

static float evaluatePixel(const ProceduralTextureSettings& settings, int x, int y)
{
    const int periodX = std::max(1, periodFromScale(settings.primary.scale[0]));
    const int periodY = std::max(1, periodFromScale(settings.primary.scale[1]));

    const float u = static_cast<float>(x) / std::max(1, settings.width);
    const float v = static_cast<float>(y) / std::max(1, settings.height);

    Float2 p = {
        u * static_cast<float>(periodX) + settings.primary.offset[0],
        v * static_cast<float>(periodY) + settings.primary.offset[1]
    };

    if (settings.post.domainWarpEnabled)
    {
        const auto& warp = settings.post.warp;
        const int warpPeriodX = std::max(1, periodFromScale(warp.scale[0]));
        const int warpPeriodY = std::max(1, periodFromScale(warp.scale[1]));
        const Float2 wp = {
            u * static_cast<float>(warpPeriodX) + warp.offset[0],
            v * static_cast<float>(warpPeriodY) + warp.offset[1]
        };
        const float wx = sampleBaseGenerator(warp, wp, warpPeriodX, warpPeriodY);
        const float wy = sampleBaseGenerator(warp, add(wp, {19.13f, 7.91f}), warpPeriodX, warpPeriodY);
        p.x += (wx - 0.5f) * 2.0f * settings.post.warpAmplitude;
        p.y += (wy - 0.5f) * 2.0f * settings.post.warpAmplitude;
    }

    float value = sampleBaseGenerator(settings.primary, p, periodX, periodY);
    value *= settings.primary.amplitude;

    if (settings.post.useSecondary)
    {
        const auto& secondary = settings.post.secondary;
        const int secondaryPeriodX = std::max(1, periodFromScale(secondary.scale[0]));
        const int secondaryPeriodY = std::max(1, periodFromScale(secondary.scale[1]));
        const Float2 sp = {
            u * static_cast<float>(secondaryPeriodX) + secondary.offset[0],
            v * static_cast<float>(secondaryPeriodY) + secondary.offset[1]
        };
        float secondaryValue = sampleBaseGenerator(secondary, sp, secondaryPeriodX, secondaryPeriodY);
        secondaryValue *= secondary.amplitude;
        value = applyBlend(value, secondaryValue, settings.post.blendMode, settings.post.blendWeight);
    }

    return applyPost(settings, value);
}

static void fillOutputs(const ProceduralTextureSettings& settings,
                        const std::vector<float>& values,
                        ProceduralTextureOutput& output)
{
    const size_t pixelCount = static_cast<size_t>(std::max(0, settings.width)) * static_cast<size_t>(std::max(0, settings.height));
    if ((settings.outputFormat == ProceduralTextureOutputFormat::Float32 || settings.outputFormat == ProceduralTextureOutputFormat::Both) && output.rgba32f.empty())
    {
        output.rgba32f.resize(pixelCount * 4);
    }
    if ((settings.outputFormat == ProceduralTextureOutputFormat::Rgba8 || settings.outputFormat == ProceduralTextureOutputFormat::Both) && output.rgba8.empty())
    {
        output.rgba8.resize(pixelCount * 4);
    }

    for (size_t i = 0; i < pixelCount; ++i)
    {
        const float v = clamp01(values[i]);
        if (!output.rgba32f.empty())
        {
            const size_t o = i * 4;
            output.rgba32f[o + 0] = v;
            output.rgba32f[o + 1] = v;
            output.rgba32f[o + 2] = v;
            output.rgba32f[o + 3] = 1.0f;
        }
        if (!output.rgba8.empty())
        {
            const std::uint8_t c = static_cast<std::uint8_t>(std::lround(v * 255.0f));
            const size_t o = i * 4;
            output.rgba8[o + 0] = c;
            output.rgba8[o + 1] = c;
            output.rgba8[o + 2] = c;
            output.rgba8[o + 3] = 255u;
        }
    }
}

} // namespace

void ProceduralTextureOutput::clear()
{
    width = 0;
    height = 0;
    rgba8.clear();
    rgba32f.clear();
}

std::unique_ptr<ImageF32x4_RGBA> ProceduralTextureOutput::toFloatImage() const
{
    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }

    auto img = std::make_unique<ImageF32x4_RGBA>();
    if (!rgba32f.empty())
    {
        img->setFromRGBA32F(rgba32f.data(), width, height);
        return img;
    }
    if (!rgba8.empty())
    {
        img->setFromRGBA8(rgba8.data(), width, height);
        return img;
    }

    return nullptr;
}

ProceduralTextureOutput ProceduralTextureGenerator::generate(const ProceduralTextureSettings& settings)
{
    ProceduralTextureOutput output;
    output.width = std::max(0, settings.width);
    output.height = std::max(0, settings.height);

    if (output.width == 0 || output.height == 0)
    {
        return output;
    }

    const size_t pixelCount = static_cast<size_t>(output.width) * static_cast<size_t>(output.height);
    std::vector<float> values(pixelCount, 0.0f);

    auto worker = [&](int y0, int y1)
    {
        for (int y = y0; y < y1; ++y)
        {
            for (int x = 0; x < output.width; ++x)
            {
                values[static_cast<size_t>(y) * static_cast<size_t>(output.width) + static_cast<size_t>(x)] = evaluatePixel(settings, x, y);
            }
        }
    };

    if (settings.parallel && output.height >= 64)
    {
        const unsigned int hardware = std::max(1u, std::thread::hardware_concurrency());
        const int threadCount = std::min<int>(static_cast<int>(hardware), output.height);
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(threadCount));
        const int stripe = (output.height + threadCount - 1) / threadCount;
        for (int i = 0; i < threadCount; ++i)
        {
            const int y0 = i * stripe;
            const int y1 = std::min(output.height, y0 + stripe);
            if (y0 >= y1)
            {
                break;
            }
            threads.emplace_back(worker, y0, y1);
        }
        for (auto& t : threads)
        {
            t.join();
        }
    }
    else
    {
        worker(0, output.height);
    }

    if (settings.post.normalize)
    {
        auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
        const float minValue = *minIt;
        const float maxValue = *maxIt;
        const float denom = std::max(maxValue - minValue, 1e-6f);
        for (float& v : values)
        {
            v = clamp01((v - minValue) / denom);
        }
    }

    fillOutputs(settings, values, output);
    return output;
}

bool ProceduralTextureGenerator::generate(const ProceduralTextureSettings& settings, ImageF32x4_RGBA& output)
{
    const ProceduralTextureOutput result = generate(settings);
    if (result.width <= 0 || result.height <= 0)
    {
        return false;
    }

    if (!result.rgba32f.empty())
    {
        output.setFromRGBA32F(result.rgba32f.data(), result.width, result.height);
        return true;
    }

    if (!result.rgba8.empty())
    {
        output.setFromRGBA8(result.rgba8.data(), result.width, result.height);
        return true;
    }

    return false;
}

ProceduralTextureSettings ProceduralTextureGenerator::makePreset(ProceduralTexturePreset preset, std::uint32_t seed)
{
    ProceduralTextureSettings settings;
    settings.primary.seed = seed;
    settings.post.secondary.seed = seed + 101u;
    settings.post.warp.seed = seed + 211u;
    settings.primary.scale = {8.0f, 8.0f};
    settings.primary.offset = {0.0f, 0.0f};
    settings.primary.rotation = 0.0f;
    settings.primary.amplitude = 1.0f;
    settings.primary.octaves = 5;
    settings.primary.lacunarity = 2.0f;
    settings.primary.gain = 0.5f;
    settings.primary.cellJitter = 0.75f;
    settings.outputFormat = ProceduralTextureOutputFormat::Both;
    settings.seamless = true;

    switch (preset)
    {
    case ProceduralTexturePreset::Marble:
        settings.primary.kind = ProceduralTextureGeneratorKind::FBM;
        settings.primary.scale = {5.0f, 8.0f};
        settings.primary.octaves = 6;
        settings.primary.rotation = 0.35f;
        settings.post.useSecondary = true;
        settings.post.secondary.kind = ProceduralTextureGeneratorKind::White;
        settings.post.blendMode = ProceduralTextureBlendMode::Overlay;
        settings.post.blendWeight = 0.28f;
        break;
    case ProceduralTexturePreset::Clouds:
        settings.primary.kind = ProceduralTextureGeneratorKind::FBM;
        settings.primary.scale = {3.5f, 3.5f};
        settings.primary.octaves = 7;
        settings.primary.gain = 0.55f;
        settings.post.normalize = true;
        settings.post.normalizeMin = 0.25f;
        settings.post.normalizeMax = 0.75f;
        settings.post.clampMin = 0.0f;
        settings.post.clampMax = 1.0f;
        break;
    case ProceduralTexturePreset::Cellular:
        settings.primary.kind = ProceduralTextureGeneratorKind::Voronoi;
        settings.primary.voronoiMode = ProceduralTextureVoronoiMode::Distance;
        settings.primary.scale = {12.0f, 12.0f};
        settings.primary.cellJitter = 0.9f;
        break;
    case ProceduralTexturePreset::Fabric:
        settings.primary.kind = ProceduralTextureGeneratorKind::Value;
        settings.primary.scale = {24.0f, 24.0f};
        settings.post.domainWarpEnabled = true;
        settings.post.warp.kind = ProceduralTextureGeneratorKind::Perlin;
        settings.post.warp.scale = {6.0f, 6.0f};
        settings.post.warp.amplitude = 1.0f;
        settings.post.warp.rotation = 0.2f;
        settings.post.warpAmplitude = 0.18f;
        break;
    case ProceduralTexturePreset::Terrain:
        settings.primary.kind = ProceduralTextureGeneratorKind::FBM;
        settings.primary.scale = {4.0f, 4.0f};
        settings.primary.octaves = 6;
        settings.post.useSecondary = true;
        settings.post.secondary.kind = ProceduralTextureGeneratorKind::Voronoi;
        settings.post.secondary.voronoiMode = ProceduralTextureVoronoiMode::Edge;
        settings.post.secondary.scale = {7.0f, 7.0f};
        settings.post.secondary.amplitude = 0.25f;
        settings.post.blendMode = ProceduralTextureBlendMode::Multiply;
        settings.post.blendWeight = 0.35f;
        break;
    case ProceduralTexturePreset::Metal:
        settings.primary.kind = ProceduralTextureGeneratorKind::White;
        settings.primary.scale = {48.0f, 48.0f};
        settings.post.useSecondary = true;
        settings.post.secondary.kind = ProceduralTextureGeneratorKind::Gradient;
        settings.post.secondary.gradientMode = ProceduralTextureGradientMode::Linear;
        settings.post.secondary.rotation = 1.1f;
        settings.post.blendMode = ProceduralTextureBlendMode::Add;
        settings.post.blendWeight = 0.18f;
        settings.post.gamma = 1.4f;
        break;
    }

    return settings;
}

struct ProceduralTextureComputePipeline::Impl
{
    RefCntAutoPtr<IBuffer> paramsCB_;
    std::unique_ptr<ComputeExecutor> executor_;
};

struct alignas(16) ProceduralTextureGpuCB
{
    std::array<std::uint32_t, 4> head0 {};
    std::array<std::uint32_t, 4> head1 {};
    std::array<float, 4> primary0 {};
    std::array<float, 4> primary1 {};
    std::array<float, 4> post0 {};
    std::array<float, 4> post1 {};
    std::array<float, 4> primary2 {};
    std::array<float, 4> secondary0 {};
    std::array<float, 4> secondary1 {};
    std::array<std::uint32_t, 4> secondary2 {};
    std::array<std::uint32_t, 4> secondary3 {};
    std::array<float, 4> warp0 {};
    std::array<float, 4> warp1 {};
    std::array<float, 4> warp2 {};
};

static ProceduralTextureGpuCB packGpuCB(const ProceduralTextureSettings& settings)
{
    ProceduralTextureGpuCB cb {};
    cb.head0 = {
        static_cast<std::uint32_t>(std::max(0, settings.width)),
        static_cast<std::uint32_t>(std::max(0, settings.height)),
        static_cast<std::uint32_t>(settings.primary.kind),
        static_cast<std::uint32_t>(settings.primary.voronoiMode)
    };
    cb.head1 = {
        static_cast<std::uint32_t>(settings.primary.gradientMode),
        static_cast<std::uint32_t>(settings.post.blendMode),
        (settings.post.normalize ? 1u : 0u) |
            (settings.post.invert ? 2u : 0u) |
            (settings.post.clampEnabled ? 4u : 0u) |
            (settings.post.remapEnabled ? 8u : 0u) |
            (settings.post.useSecondary ? 16u : 0u) |
            (settings.post.domainWarpEnabled ? 32u : 0u),
        std::max(1u, settings.primary.octaves)
    };
    cb.primary0 = {
        settings.primary.scale[0], settings.primary.scale[1],
        settings.primary.offset[0], settings.primary.offset[1]
    };
    cb.primary1 = {
        settings.primary.rotation, settings.primary.amplitude,
        settings.primary.lacunarity, settings.primary.gain
    };
    cb.post0 = {
        settings.post.normalizeMin, settings.post.normalizeMax,
        settings.post.clampMin, settings.post.clampMax
    };
    cb.post1 = {
        settings.post.remapInMin, settings.post.remapInMax,
        settings.post.remapOutMin, settings.post.remapOutMax
    };
    cb.primary2 = {
        settings.post.gamma, settings.post.blendWeight,
        settings.post.warpAmplitude, settings.primary.cellJitter
    };
    cb.secondary0 = {
        settings.post.secondary.scale[0], settings.post.secondary.scale[1],
        settings.post.secondary.offset[0], settings.post.secondary.offset[1]
    };
    cb.secondary1 = {
        settings.post.secondary.rotation, settings.post.secondary.amplitude,
        settings.post.secondary.lacunarity, settings.post.secondary.gain
    };
    cb.secondary2 = {
        static_cast<std::uint32_t>(settings.post.secondary.kind),
        static_cast<std::uint32_t>(settings.post.secondary.voronoiMode),
        static_cast<std::uint32_t>(settings.post.secondary.gradientMode),
        std::max(1u, settings.post.secondary.octaves)
    };
    cb.secondary3 = {
        settings.post.secondary.seed,
        settings.primary.seed,
        static_cast<std::uint32_t>(settings.post.warp.kind),
        0u
    };
    cb.warp0 = {
        settings.post.warp.scale[0], settings.post.warp.scale[1],
        settings.post.warp.offset[0], settings.post.warp.offset[1]
    };
    cb.warp1 = {
        settings.post.warp.rotation, settings.post.warp.gain,
        settings.post.warp.lacunarity, settings.post.warp.amplitude
    };
    cb.warp2 = {
        settings.post.warp.cellJitter, 0.0f, 0.0f, 0.0f
    };
    return cb;
}

ProceduralTextureComputePipeline::ProceduralTextureComputePipeline(GpuContext& context)
    : context_(context), pImpl_(new Impl())
{
}

ProceduralTextureComputePipeline::~ProceduralTextureComputePipeline()
{
    delete pImpl_;
}

bool ProceduralTextureComputePipeline::initialize()
{
    auto pDevice = context_.RenderDevice();
    if (!pDevice)
    {
        return false;
    }

    BufferDesc buffDesc;
    buffDesc.Name = "ProceduralTextureCB";
    buffDesc.Size = sizeof(ProceduralTextureGpuCB);
    buffDesc.Usage = USAGE_DYNAMIC;
    buffDesc.BindFlags = BIND_UNIFORM_BUFFER;
    buffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->paramsCB_);
    if (!pImpl_->paramsCB_)
    {
        qWarning() << "[ProceduralTextureComputePipeline] failed to create constant buffer";
        return false;
    }

    static const ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "OutTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };

    pImpl_->executor_ = std::make_unique<ComputeExecutor>(context_);
    ComputePipelineDesc desc;
    desc.name = "ProceduralTexturePipeline";
    desc.shaderSource = Shaders::ProceduralTexture::ProceduralTextureShaderText;
    desc.entryPoint = "CSMain";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = static_cast<Uint32>(sizeof(vars) / sizeof(vars[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (!pImpl_->executor_->build(desc))
    {
        qWarning() << "[ProceduralTextureComputePipeline] failed to build compute PSO";
        return false;
    }

    if (!pImpl_->executor_->createShaderResourceBinding(true))
    {
        qWarning() << "[ProceduralTextureComputePipeline] failed to create SRB";
        return false;
    }

    if (!pImpl_->executor_->setBuffer("ProceduralTextureCB", pImpl_->paramsCB_))
    {
        qWarning() << "[ProceduralTextureComputePipeline] failed to bind constant buffer";
        return false;
    }

    return true;
}

bool ProceduralTextureComputePipeline::generate(IDeviceContext* ctx, ITextureView* outputUAV, const ProceduralTextureSettings& settings)
{
    if (!ctx || !outputUAV || !ready())
    {
        return false;
    }

    const ProceduralTextureGpuCB gpuCB = packGpuCB(settings);
    void* pData = nullptr;
    ctx->MapBuffer(pImpl_->paramsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData)
    {
        return false;
    }

    std::memcpy(pData, &gpuCB, sizeof(gpuCB));
    ctx->UnmapBuffer(pImpl_->paramsCB_, MAP_WRITE);

    pImpl_->executor_->setTextureView("OutTex", outputUAV);
    const auto* outTex = outputUAV->GetTexture();
    if (!outTex)
    {
        return false;
    }

    const auto& desc = outTex->GetDesc();
    const auto attribs = ComputeExecutor::makeDispatchAttribs(desc.Width, desc.Height, 1, 8, 8, 1);
    pImpl_->executor_->dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return true;
}

bool ProceduralTextureComputePipeline::ready() const
{
    return pImpl_ && pImpl_->paramsCB_ && pImpl_->executor_ && pImpl_->executor_->ready();
}

ITexture* ProceduralTextureComputePipeline::createOutputTexture(IRenderDevice* device,
                                                                std::uint32_t width,
                                                                std::uint32_t height,
                                                                TEXTURE_FORMAT format,
                                                                const char* name)
{
    if (!device || width == 0 || height == 0)
    {
        return nullptr;
    }

    TextureDesc desc;
    desc.Name = name ? name : "ProceduralTexture";
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleCount = 1;
    desc.Format = format;
    desc.Usage = USAGE_DEFAULT;
    desc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = CPU_ACCESS_NONE;

    ITexture* texture = nullptr;
    device->CreateTexture(desc, nullptr, &texture);
    return texture;
}

} // namespace ArtifactCore




