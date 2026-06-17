module;
#include <vector>
#include <algorithm>
#include <cmath>

module Graphics.Effect.Creative.ColorVibrance;

namespace ArtifactCore {

namespace {

constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float saturationOf(float r, float g, float b) {
    const float cmax = std::max({r, g, b});
    const float cmin = std::min({r, g, b});
    return cmax - cmin;
}

float vibranceWeight(float saturation, float amount) {
    const float lowSatBoost = 1.0f - saturation;
    return 1.0f + amount * lowSatBoost;
}

} // namespace

ColorVibranceEffect::ColorVibranceEffect() {
    parameters_.push_back({"Vibrance", "Vibrance Amount", EffectParameterType::Float, 0.0f, -1.0f, 1.0f});
    parameters_.push_back({"Saturation", "Saturation", EffectParameterType::Float, 0.0f, -1.0f, 1.0f});
    parameters_.push_back({"ColorBoost", "Color Boost", EffectParameterType::Float, 1.0f, 0.0f, 2.0f});
    parameters_.push_back({"MatteAmount", "Matte Alpha", EffectParameterType::Float, 0.0f, 0.0f, 1.0f});
    parameters_.push_back({"MatteThreshold", "Matte Threshold", EffectParameterType::Float, 0.35f, 0.0f, 1.0f});
    parameters_.push_back({"MatteSoftness", "Matte Softness", EffectParameterType::Float, 0.20f, 0.0f, 1.0f});
}

void ColorVibranceEffect::process(VideoFrame& frame, const CreativeEffectContext&) {
    if (!enabled_) {
        return;
    }

    auto rCh = frame.getChannel(ChannelType::Red);
    auto gCh = frame.getChannel(ChannelType::Green);
    auto bCh = frame.getChannel(ChannelType::Blue);
    auto aCh = frame.getChannel(ChannelType::Alpha);
    if (!rCh || !gCh || !bCh) {
        return;
    }

    const int width = frame.width();
    const int height = frame.height();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count == 0) {
        return;
    }

    const float vib = vibrance();
    const float sat = saturation();
    const float boost = colorBoost();
    const float matteMix = matteAmount();
    const float matteT = matteThreshold();
    const float matteS = std::max(0.0001f, matteSoftness());

    auto* rData = rCh->data();
    auto* gData = gCh->data();
    auto* bData = bCh->data();
    auto* aData = aCh ? aCh->data() : nullptr;

    for (std::size_t i = 0; i < count; ++i) {
        const float r = rData[i];
        const float g = gData[i];
        const float b = bData[i];

        const float luma = r * kLumaR + g * kLumaG + b * kLumaB;
        const float currentSat = saturationOf(r, g, b);
        const float vibScale = vibranceWeight(currentSat, vib);
        const float satScale = 1.0f + sat;
        const float combinedScale = std::max(0.0f, boost * vibScale * satScale);

        const float nr = luma + (r - luma) * combinedScale;
        const float ng = luma + (g - luma) * combinedScale;
        const float nb = luma + (b - luma) * combinedScale;

        rData[i] = clamp01(nr);
        gData[i] = clamp01(ng);
        bData[i] = clamp01(nb);

        if (aData && matteMix > 0.0f) {
            const float matteBase = (currentSat - matteT) / matteS;
            const float matte = clamp01(matteBase);
            aData[i] = clamp01(std::lerp(aData[i], matte, matteMix));
        }
    }
}

} // namespace ArtifactCore
