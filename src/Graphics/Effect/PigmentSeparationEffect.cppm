module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.PigmentSeparation;

import Channel;
import Math.Noise;

namespace ArtifactCore {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float bilinearSample(const std::vector<float>& channel, int width, int height, float x, float y) {
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

float lumaAt(const std::vector<float>& r, const std::vector<float>& g, const std::vector<float>& b, int width, int height, float x, float y) {
    return bilinearSample(r, width, height, x, y) * kLumaR +
           bilinearSample(g, width, height, x, y) * kLumaG +
           bilinearSample(b, width, height, x, y) * kLumaB;
}

} // namespace

PigmentSeparationEffect::PigmentSeparationEffect() {
    parameters_.push_back({"Spread", "Pigment Spread", EffectParameterType::Float, 0.42f, 0.0f, 1.0f});
    parameters_.push_back({"Bleed", "Wet Bleed", EffectParameterType::Float, 0.48f, 0.0f, 1.0f});
    parameters_.push_back({"Flow", "Flow Direction", EffectParameterType::Float, 0.35f, 0.0f, 1.0f});
    parameters_.push_back({"Granulation", "Paper Grain", EffectParameterType::Float, 0.28f, 0.0f, 1.0f});
}

void PigmentSeparationEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto rCh = frame.getChannel(ChannelType::Red);
    auto gCh = frame.getChannel(ChannelType::Green);
    auto bCh = frame.getChannel(ChannelType::Blue);
    if (!rCh || !gCh || !bCh) return;

    const int width = frame.width();
    const int height = frame.height();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count == 0) return;

    std::vector<float> sourceR(rCh->data(), rCh->data() + count);
    std::vector<float> sourceG(gCh->data(), gCh->data() + count);
    std::vector<float> sourceB(bCh->data(), bCh->data() + count);

    const float spreadAmount = spread();
    const float bleedAmount = bleed();
    const float flowAmount = flow();
    const float grainAmount = granulation();
    const float time = static_cast<float>(context.time);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;

            const float currentR = sourceR[idx];
            const float currentG = sourceG[idx];
            const float currentB = sourceB[idx];
            const float currentLuma = currentR * kLumaR + currentG * kLumaG + currentB * kLumaB;

            const float leftLuma = lumaAt(sourceR, sourceG, sourceB, width, height, static_cast<float>(x - 1), static_cast<float>(y));
            const float rightLuma = lumaAt(sourceR, sourceG, sourceB, width, height, static_cast<float>(x + 1), static_cast<float>(y));
            const float upLuma = lumaAt(sourceR, sourceG, sourceB, width, height, static_cast<float>(x), static_cast<float>(y - 1));
            const float downLuma = lumaAt(sourceR, sourceG, sourceB, width, height, static_cast<float>(x), static_cast<float>(y + 1));

            const float gradientX = rightLuma - leftLuma;
            const float gradientY = downLuma - upLuma;
            const float localContrast = std::abs(gradientX) + std::abs(gradientY);

            const float channelMin = std::min({currentR, currentG, currentB});
            const float channelMax = std::max({currentR, currentG, currentB});
            const float saturation = channelMax - channelMin;

            const float flowNoise = NoiseGenerator::perlin(x * 0.013f, y * 0.013f, time * 0.18f);
            const float angle = flowNoise * kPi + currentLuma * kPi * flowAmount;
            const float flowX = std::cos(angle) + gradientY * 1.2f * flowAmount;
            const float flowY = std::sin(angle) - gradientX * 1.2f * flowAmount;
            const float offset = (1.5f + saturation * 4.0f + localContrast * 8.0f) * spreadAmount;

            const float redSample = bilinearSample(sourceR, width, height,
                                                   x - flowX * offset - gradientY * bleedAmount * 3.0f,
                                                   y - flowY * offset + gradientX * bleedAmount * 3.0f);
            const float greenSample = bilinearSample(sourceG, width, height,
                                                     x + gradientX * offset * 0.5f,
                                                     y + gradientY * offset * 0.5f);
            const float blueSample = bilinearSample(sourceB, width, height,
                                                    x + flowX * offset + gradientY * bleedAmount * 3.0f,
                                                    y + flowY * offset - gradientX * bleedAmount * 3.0f);

            const float bleedR = bilinearSample(sourceR, width, height, x + flowX * bleedAmount * 2.0f, y + flowY * bleedAmount * 2.0f);
            const float bleedG = bilinearSample(sourceG, width, height, x, y);
            const float bleedB = bilinearSample(sourceB, width, height, x - flowX * bleedAmount * 2.0f, y - flowY * bleedAmount * 2.0f);

            const float wetMask = clamp01(0.20f + saturation * 0.85f + localContrast * 1.4f);
            const float paper = 0.85f + (NoiseGenerator::perlin(x * 0.08f, y * 0.08f, 30.0f + time * 0.05f) - 0.5f) * grainAmount * 0.35f;

            const float mixedR = std::lerp(redSample, bleedR, bleedAmount * wetMask * 0.55f);
            const float mixedG = std::lerp(greenSample, bleedG, bleedAmount * wetMask * 0.35f);
            const float mixedB = std::lerp(blueSample, bleedB, bleedAmount * wetMask * 0.55f);

            const float finalMix = clamp01(0.28f + spreadAmount * 0.42f + wetMask * 0.25f);
            rCh->data()[idx] = clamp01(std::lerp(currentR, mixedR * paper, finalMix));
            gCh->data()[idx] = clamp01(std::lerp(currentG, mixedG * paper, finalMix));
            bCh->data()[idx] = clamp01(std::lerp(currentB, mixedB * paper, finalMix));
        }
    }
}

} // namespace ArtifactCore
