module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.EdgeEcho;

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

EdgeEchoEffect::EdgeEchoEffect() {
    parameters_.push_back({"Intensity", "Echo Intensity", EffectParameterType::Float, 0.62f, 0.0f, 1.0f});
    parameters_.push_back({"Thickness", "Echo Thickness", EffectParameterType::Float, 2.0f, 0.5f, 8.0f});
    parameters_.push_back({"EchoCount", "Echo Count", EffectParameterType::Float, 3.0f, 1.0f, 6.0f});
    parameters_.push_back({"Tint", "Edge Tint", EffectParameterType::Float, 0.22f, 0.0f, 1.0f});
}

void EdgeEchoEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    (void)context;
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

    const float strength = intensity();
    const float thicknessPx = thickness();
    const int echoSteps = std::max(1, static_cast<int>(std::round(echoCount())));
    const float tintAmount = tint();

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
            const float edgeStrength = clamp01(gradientLen * 2.3f + std::abs(lC - 0.5f) * 0.15f);

            float ux = 0.0f;
            float uy = 0.0f;
            if (gradientLen > 1e-5f) {
                ux = gx / gradientLen;
                uy = gy / gradientLen;
            }

            float echoR = srcR[idx];
            float echoG = srcG[idx];
            float echoB = srcB[idx];
            float totalWeight = 1.0f;

            for (int step = 1; step <= echoSteps; ++step) {
                const float offset = thicknessPx * static_cast<float>(step);
                const float sx = static_cast<float>(x) + ux * offset;
                const float sy = static_cast<float>(y) + uy * offset;
                const float sampleEdge = clamp01(luminanceAt(srcR, srcG, srcB, width, height, sx, sy) * 1.5f + edgeStrength * 0.75f);
                const float weight = strength / static_cast<float>(step);

                echoR += sampleBilinear(srcR, width, height, sx, sy) * weight;
                echoG += sampleBilinear(srcG, width, height, sx, sy) * weight;
                echoB += sampleBilinear(srcB, width, height, sx, sy) * weight;
                totalWeight += weight;

                const float glow = sampleEdge * weight * 0.55f;
                echoR += glow * (0.5f + tintAmount);
                echoG += glow * (0.45f + tintAmount * 0.3f);
                echoB += glow * (0.55f + tintAmount);
            }

            const float edgeMix = clamp01(edgeStrength * strength);
            const float softR = echoR / totalWeight;
            const float softG = echoG / totalWeight;
            const float softB = echoB / totalWeight;

            rCh->data()[idx] = clamp01(std::lerp(srcR[idx], softR + edgeStrength * tintAmount, edgeMix));
            gCh->data()[idx] = clamp01(std::lerp(srcG[idx], softG + edgeStrength * tintAmount * 0.6f, edgeMix));
            bCh->data()[idx] = clamp01(std::lerp(srcB[idx], softB + edgeStrength * tintAmount, edgeMix));
        }
    }
}

} // namespace ArtifactCore
