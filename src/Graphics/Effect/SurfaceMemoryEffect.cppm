module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.SurfaceMemory;

import Channel;
import Math.Noise;

namespace ArtifactCore {

namespace {

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

} // namespace

SurfaceMemoryEffect::SurfaceMemoryEffect() {
    parameters_.push_back({"Retention", "Memory Retention", EffectParameterType::Float, 0.62f, 0.0f, 1.0f});
    parameters_.push_back({"Refresh", "Surface Refresh", EffectParameterType::Float, 0.32f, 0.0f, 1.0f});
    parameters_.push_back({"Texture", "Patina Texture", EffectParameterType::Float, 0.36f, 0.0f, 1.0f});
    parameters_.push_back({"Smear", "Directional Smear", EffectParameterType::Float, 0.24f, 0.0f, 1.0f});
}

void SurfaceMemoryEffect::ensureMemorySize(int width, int height) {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (memoryWidth_ == width && memoryHeight_ == height &&
        memoryR_.size() == count && memoryG_.size() == count && memoryB_.size() == count) {
        return;
    }

    memoryWidth_ = width;
    memoryHeight_ = height;
    memoryR_.assign(count, 0.0f);
    memoryG_.assign(count, 0.0f);
    memoryB_.assign(count, 0.0f);
    lastFrameIndex_ = -1;
}

void SurfaceMemoryEffect::resetMemoryFromSource(const float* r, const float* g, const float* b, std::size_t count) {
    memoryR_.assign(r, r + count);
    memoryG_.assign(g, g + count);
    memoryB_.assign(b, b + count);
}

void SurfaceMemoryEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto rCh = frame.getChannel(ChannelType::Red);
    auto gCh = frame.getChannel(ChannelType::Green);
    auto bCh = frame.getChannel(ChannelType::Blue);
    if (!rCh || !gCh || !bCh) return;

    const int width = frame.width();
    const int height = frame.height();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count == 0) return;

    ensureMemorySize(width, height);

    std::vector<float> sourceR(rCh->data(), rCh->data() + count);
    std::vector<float> sourceG(gCh->data(), gCh->data() + count);
    std::vector<float> sourceB(bCh->data(), bCh->data() + count);

    if (lastFrameIndex_ < 0 || context.frameIndex < lastFrameIndex_) {
        resetMemoryFromSource(sourceR.data(), sourceG.data(), sourceB.data(), count);
        lastFrameIndex_ = context.frameIndex;
        return;
    }

    const float retentionAmount = retention();
    const float refreshAmount = refresh();
    const float textureStrength = textureAmount();
    const float smearAmount = smear();
    const float time = static_cast<float>(context.time);

    std::vector<float> nextMemoryR = memoryR_;
    std::vector<float> nextMemoryG = memoryG_;
    std::vector<float> nextMemoryB = memoryB_;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;

            const float srcR = sourceR[idx];
            const float srcG = sourceG[idx];
            const float srcB = sourceB[idx];

            const float luma = srcR * kLumaR + srcG * kLumaG + srcB * kLumaB;
            const float colorMin = std::min({srcR, srcG, srcB});
            const float colorMax = std::max({srcR, srcG, srcB});
            const float saturation = colorMax - colorMin;

            const float patinaNoise = NoiseGenerator::perlin(x * 0.018f, y * 0.018f, time * 0.07f);
            const float smearNoiseX = NoiseGenerator::perlin(x * 0.010f, y * 0.010f, 20.0f + time * 0.11f);
            const float smearNoiseY = NoiseGenerator::perlin(x * 0.010f, y * 0.010f, 60.0f + time * 0.11f);
            const float driftX = smearNoiseX * smearAmount * 5.0f;
            const float driftY = smearNoiseY * smearAmount * 5.0f;

            const float memorySampleR = bilinearSample(memoryR_, width, height, x - driftX, y - driftY);
            const float memorySampleG = bilinearSample(memoryG_, width, height, x - driftX, y - driftY);
            const float memorySampleB = bilinearSample(memoryB_, width, height, x - driftX, y - driftY);

            const float surfacePattern = clamp01(0.5f + patinaNoise * 0.5f);
            const float writeMask = clamp01(0.12f + luma * 0.48f + saturation * 0.40f);
            const float updateAmount = refreshAmount * (0.20f + writeMask * (0.55f + textureStrength * 0.25f));

            nextMemoryR[idx] = clamp01(std::lerp(memorySampleR, srcR, updateAmount));
            nextMemoryG[idx] = clamp01(std::lerp(memorySampleG, srcG, updateAmount));
            nextMemoryB[idx] = clamp01(std::lerp(memorySampleB, srcB, updateAmount));

            const float residue = retentionAmount * (0.22f + textureStrength * 0.28f + (1.0f - writeMask) * 0.30f);
            const float textureMix = 0.65f + surfacePattern * textureStrength * 0.35f;

            rCh->data()[idx] = clamp01(std::lerp(srcR, nextMemoryR[idx] * textureMix, residue));
            gCh->data()[idx] = clamp01(std::lerp(srcG, nextMemoryG[idx] * textureMix, residue));
            bCh->data()[idx] = clamp01(std::lerp(srcB, nextMemoryB[idx] * textureMix, residue));
        }
    }

    memoryR_ = std::move(nextMemoryR);
    memoryG_ = std::move(nextMemoryG);
    memoryB_ = std::move(nextMemoryB);
    lastFrameIndex_ = context.frameIndex;
}

} // namespace ArtifactCore
