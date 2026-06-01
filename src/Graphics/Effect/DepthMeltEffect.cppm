module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.DepthMelt;

import Channel;
import Math.Noise;

namespace ArtifactCore {

namespace {

constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float sampleBilinear(const std::vector<float>& channel, int width, int height, float x, float y) {
    const float fx = std::clamp(x, 0.0f, static_cast<float>(width - 1));
    const float fy = std::clamp(y, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const float c00 = channel[y0 * width + x0];
    const float c10 = channel[y0 * width + x1];
    const float c01 = channel[y1 * width + x0];
    const float c11 = channel[y1 * width + x1];

    const float top = std::lerp(c00, c10, tx);
    const float bottom = std::lerp(c01, c11, tx);
    return std::lerp(top, bottom, ty);
}

float luminanceAt(const std::vector<float>& r, const std::vector<float>& g, const std::vector<float>& b, int width, int height, float x, float y) {
    return sampleBilinear(r, width, height, x, y) * kLumaR +
           sampleBilinear(g, width, height, x, y) * kLumaG +
           sampleBilinear(b, width, height, x, y) * kLumaB;
}

} // namespace

DepthMeltEffect::DepthMeltEffect() {
    parameters_.push_back({"Melt", "Melt Strength", EffectParameterType::Float, 0.56f, 0.0f, 1.0f});
    parameters_.push_back({"Gravity", "Gravity Pull", EffectParameterType::Float, 0.48f, 0.0f, 1.0f});
    parameters_.push_back({"Heat", "Heat Haze", EffectParameterType::Float, 0.34f, 0.0f, 1.0f});
    parameters_.push_back({"Detail", "Surface Detail", EffectParameterType::Float, 0.36f, 0.0f, 1.0f});
}

void DepthMeltEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto rCh = frame.getChannel(ChannelType::Red);
    auto gCh = frame.getChannel(ChannelType::Green);
    auto bCh = frame.getChannel(ChannelType::Blue);
    if (!rCh || !gCh || !bCh) return;

    const int width = frame.width();
    const int height = frame.height();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count == 0) return;

    std::vector<float> srcR(rCh->data(), rCh->data() + count);
    std::vector<float> srcG(gCh->data(), gCh->data() + count);
    std::vector<float> srcB(bCh->data(), bCh->data() + count);

    const float melt = meltAmount();
    const float gravityPull = gravity();
    const float heat = heat();
    const float surfaceDetail = detail();
    const float time = static_cast<float>(context.time);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;

            const float lC = luminanceAt(srcR, srcG, srcB, width, height, static_cast<float>(x), static_cast<float>(y));
            const float lL = luminanceAt(srcR, srcG, srcB, width, height, static_cast<float>(x - 1), static_cast<float>(y));
            const float lR = luminanceAt(srcR, srcG, srcB, width, height, static_cast<float>(x + 1), static_cast<float>(y));
            const float lU = luminanceAt(srcR, srcG, srcB, width, height, static_cast<float>(x), static_cast<float>(y - 1));
            const float lD = luminanceAt(srcR, srcG, srcB, width, height, static_cast<float>(x), static_cast<float>(y + 1));

            const float gx = lR - lL;
            const float gy = lD - lU;
            const float slope = std::sqrt(gx * gx + gy * gy);
            const float fauxDepth = clamp01(1.0f - lC);

            const float thermal = NoiseGenerator::perlin(x * 0.015f, y * 0.015f, time * 0.20f);
            const float dripNoise = NoiseGenerator::perlin(x * 0.010f, y * 0.030f, time * 0.35f + 10.0f);
            const float meltBias = std::pow(fauxDepth, 1.45f) * melt;
            const float drip = std::max(0.0f, dripNoise - 0.12f) * (0.75f + heat * 0.75f);
            const float verticalShift = (meltBias * (0.35f + slope * surfaceDetail) + drip) * (6.0f + gravityPull * 18.0f);
            const float lateralShiftX = gx * (2.0f + heat * 6.0f) * meltBias;
            const float lateralShiftY = gy * (2.0f + heat * 6.0f) * meltBias;
            const float heatWarp = (thermal - 0.5f) * heat * 3.0f;

            const float sampleX = static_cast<float>(x) + lateralShiftX + heatWarp;
            const float sampleY = static_cast<float>(y) + verticalShift + lateralShiftY + heatWarp * 0.35f;

            const float r = sampleBilinear(srcR, width, height, sampleX + meltBias * 0.8f, sampleY + verticalShift * 0.35f);
            const float g = sampleBilinear(srcG, width, height, sampleX, sampleY);
            const float b = sampleBilinear(srcB, width, height, sampleX - meltBias * 0.8f, sampleY + verticalShift * 0.18f);

            const float meltGlow = clamp01(meltBias * 0.5f + slope * 0.5f + heat * 0.2f);
            rCh->data()[idx] = clamp01(std::lerp(srcR[idx], r + meltGlow * 0.08f, meltBias));
            gCh->data()[idx] = clamp01(std::lerp(srcG[idx], g + meltGlow * 0.04f, meltBias));
            bCh->data()[idx] = clamp01(std::lerp(srcB[idx], b + meltGlow * 0.08f, meltBias));
        }
    }
}

} // namespace ArtifactCore
