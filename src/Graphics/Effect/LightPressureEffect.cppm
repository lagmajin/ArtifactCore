module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.LightPressure;

import Channel;

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

LightPressureEffect::LightPressureEffect() {
    parameters_.push_back({"Pressure", "Light Pressure", EffectParameterType::Float, 0.58f, 0.0f, 1.0f});
    parameters_.push_back({"Bloom", "Bloom Amount", EffectParameterType::Float, 0.36f, 0.0f, 1.0f});
    parameters_.push_back({"Spread", "Pressure Spread", EffectParameterType::Float, 0.42f, 0.0f, 1.0f});
    parameters_.push_back({"Compression", "Compression", EffectParameterType::Float, 0.30f, 0.0f, 1.0f});
}

void LightPressureEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
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

    const float pressureStrength = pressure();
    const float bloomStrength = bloom();
    const float spreadStrength = spread();
    const float compressionStrength = compression();
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
            const float gradientLen = std::sqrt(gx * gx + gy * gy);
            const float bright = clamp01(std::pow(lC, 1.45f));
            const float pulse = 0.5f + 0.5f * std::sin(time * 2.0f + bright * 6.0f);
            const float force = (bright * pressureStrength + gradientLen * 0.5f * compressionStrength) * pulse;

            float ux = 0.0f;
            float uy = 0.0f;
            if (gradientLen > 1e-5f) {
                ux = gx / gradientLen;
                uy = gy / gradientLen;
            }

            const float displacement = force * (2.0f + spreadStrength * 6.0f);
            const float sx = static_cast<float>(x) - ux * displacement - bright * compressionStrength * 1.5f;
            const float sy = static_cast<float>(y) - uy * displacement - bright * compressionStrength * 1.5f;

            const float warpedR = sampleBilinear(srcR, width, height, sx, sy);
            const float warpedG = sampleBilinear(srcG, width, height, sx, sy);
            const float warpedB = sampleBilinear(srcB, width, height, sx, sy);

            const float glow1 = sampleBilinear(srcR, width, height, sx + 1.0f, sy) * 0.299f +
                                sampleBilinear(srcG, width, height, sx + 1.0f, sy) * 0.587f +
                                sampleBilinear(srcB, width, height, sx + 1.0f, sy) * 0.114f;
            const float glow2 = sampleBilinear(srcR, width, height, sx - 1.0f, sy) * 0.299f +
                                sampleBilinear(srcG, width, height, sx - 1.0f, sy) * 0.587f +
                                sampleBilinear(srcB, width, height, sx - 1.0f, sy) * 0.114f;
            const float glow3 = sampleBilinear(srcR, width, height, sx, sy + 1.0f) * 0.299f +
                                sampleBilinear(srcG, width, height, sx, sy + 1.0f) * 0.587f +
                                sampleBilinear(srcB, width, height, sx, sy + 1.0f) * 0.114f;

            const float bloom = clamp01((glow1 + glow2 + glow3) / 3.0f * bloomStrength * bright);
            const float highlight = bloom * (0.7f + spreadStrength * 0.3f);

            rCh->data()[idx] = clamp01(std::lerp(srcR[idx], warpedR + highlight, force));
            gCh->data()[idx] = clamp01(std::lerp(srcG[idx], warpedG + highlight, force));
            bCh->data()[idx] = clamp01(std::lerp(srcB[idx], warpedB + highlight, force));
        }
    }
}

} // namespace ArtifactCore
