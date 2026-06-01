module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>

module Graphics.Effect.Creative.TemporalFossil;

import Channel;

namespace ArtifactCore {

namespace {

constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float lumaAt(const std::vector<float>& r, const std::vector<float>& g, const std::vector<float>& b, int width, int x, int y) {
    const int sx = std::clamp(x, 0, width - 1);
    const int idx = y * width + sx;
    return r[idx] * kLumaR + g[idx] * kLumaG + b[idx] * kLumaB;
}

} // namespace

TemporalFossilEffect::TemporalFossilEffect() {
    parameters_.push_back({"Persistence", "Residue Amount", EffectParameterType::Float, 0.58f, 0.0f, 1.0f});
    parameters_.push_back({"Decay", "Memory Decay", EffectParameterType::Float, 0.90f, 0.0f, 0.999f});
    parameters_.push_back({"EdgeThreshold", "Activation Threshold", EffectParameterType::Float, 0.16f, 0.0f, 0.95f});
    parameters_.push_back({"ChromaEcho", "Color Echo", EffectParameterType::Float, 0.24f, 0.0f, 1.0f});
}

void TemporalFossilEffect::ensureHistorySize(int width, int height) {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (historyWidth_ == width && historyHeight_ == height &&
        previousR_.size() == count && previousG_.size() == count && previousB_.size() == count &&
        fossilR_.size() == count && fossilG_.size() == count && fossilB_.size() == count) {
        return;
    }

    historyWidth_ = width;
    historyHeight_ = height;
    previousR_.assign(count, 0.0f);
    previousG_.assign(count, 0.0f);
    previousB_.assign(count, 0.0f);
    fossilR_.assign(count, 0.0f);
    fossilG_.assign(count, 0.0f);
    fossilB_.assign(count, 0.0f);
    lastFrameIndex_ = -1;
}

void TemporalFossilEffect::resetHistoryFromSource(const float* r, const float* g, const float* b, std::size_t count) {
    previousR_.assign(r, r + count);
    previousG_.assign(g, g + count);
    previousB_.assign(b, b + count);
    fossilR_.assign(r, r + count);
    fossilG_.assign(g, g + count);
    fossilB_.assign(b, b + count);
}

void TemporalFossilEffect::process(VideoFrame& frame, const CreativeEffectContext& context) {
    if (!enabled_) return;

    auto rCh = frame.getChannel(ChannelType::Red);
    auto gCh = frame.getChannel(ChannelType::Green);
    auto bCh = frame.getChannel(ChannelType::Blue);
    if (!rCh || !gCh || !bCh) return;

    const int width = frame.width();
    const int height = frame.height();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count == 0) return;

    ensureHistorySize(width, height);

    std::vector<float> sourceR(rCh->data(), rCh->data() + count);
    std::vector<float> sourceG(gCh->data(), gCh->data() + count);
    std::vector<float> sourceB(bCh->data(), bCh->data() + count);

    if (lastFrameIndex_ < 0 || context.frameIndex < lastFrameIndex_) {
        resetHistoryFromSource(sourceR.data(), sourceG.data(), sourceB.data(), count);
        lastFrameIndex_ = context.frameIndex;
        return;
    }

    const float persist = persistence();
    const float decayValue = decay();
    const float threshold = edgeThreshold();
    const float chroma = chromaEcho();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;

            const float currentLuma = sourceR[idx] * kLumaR + sourceG[idx] * kLumaG + sourceB[idx] * kLumaB;
            const float previousLuma = previousR_[idx] * kLumaR + previousG_[idx] * kLumaG + previousB_[idx] * kLumaB;

            const float lumLeft = lumaAt(sourceR, sourceG, sourceB, width, x - 1, y);
            const float lumRight = lumaAt(sourceR, sourceG, sourceB, width, x + 1, y);
            const float lumUp = lumaAt(sourceR, sourceG, sourceB, width, x, std::max(0, y - 1));
            const float lumDown = lumaAt(sourceR, sourceG, sourceB, width, x, std::min(height - 1, y + 1));

            const float temporalDelta = std::abs(currentLuma - previousLuma);
            const float edgeDelta = std::abs(lumRight - lumLeft) + std::abs(lumDown - lumUp);
            const float activation = clamp01((temporalDelta * 1.7f + edgeDelta * 0.45f - threshold) /
                                             std::max(0.05f, 1.0f - threshold));

            fossilR_[idx] = clamp01(fossilR_[idx] * decayValue + previousR_[idx] * activation * (0.40f + persist * 0.60f));
            fossilG_[idx] = clamp01(fossilG_[idx] * decayValue + previousG_[idx] * activation * (0.40f + persist * 0.60f));
            fossilB_[idx] = clamp01(fossilB_[idx] * decayValue + previousB_[idx] * activation * (0.40f + persist * 0.60f));

            const float echoedR = std::lerp(fossilR_[idx], fossilB_[idx], chroma * 0.35f);
            const float echoedG = fossilG_[idx];
            const float echoedB = std::lerp(fossilB_[idx], fossilR_[idx], chroma * 0.35f);
            const float blend = persist * activation;

            rCh->data()[idx] = clamp01(std::lerp(sourceR[idx], echoedR, blend));
            gCh->data()[idx] = clamp01(std::lerp(sourceG[idx], echoedG, blend));
            bCh->data()[idx] = clamp01(std::lerp(sourceB[idx], echoedB, blend));
        }
    }

    previousR_ = std::move(sourceR);
    previousG_ = std::move(sourceG);
    previousB_ = std::move(sourceB);
    lastFrameIndex_ = context.frameIndex;
}

} // namespace ArtifactCore
