module;
#include <utility>

#include <cmath>
#include <algorithm>
#include <array>

module Color.Luminance;

namespace ArtifactCore {

float ColorLuminance::calculate(float r, float g, float b, LuminanceStandard standard) {
    switch (standard) {
    case LuminanceStandard::Rec601:
        return 0.299f * r + 0.587f * g + 0.114f * b;
    case LuminanceStandard::Rec709:
    default:
        // Commonly used for sRGB / Rec.709
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    case LuminanceStandard::Rec2020:
        return 0.2627f * r + 0.6780f * g + 0.0593f * b;
    }
}

float ColorLuminance::calculatePerceptual(float r, float g, float b) {
    // 心理物理学的輝度計算 (HSP: Hue Saturation Perceptual)
    // 近似式としての HSP color model (L = sqrt(0.299*R^2 + 0.587*G^2 + 0.114*B^2))
    return std::sqrt(0.299f * r * r + 0.587f * g * g + 0.114f * b * b);
}

std::array<float, 3> ColorLuminance::toGrayscale(float r, float g, float b, LuminanceStandard standard) {
    float y = calculate(r, g, b, standard);
    return {y, y, y};
}

BroadcastSafeResult ColorLuminance::inspectBroadcastSafe(
    float r, float g, float b, LuminanceStandard standard,
    float legalBlack, float legalWhite, float channelMin, float channelMax) {
    const float luminance = calculate(r, g, b, standard);
    const bool invalidRange = !std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b);
    const bool luminanceViolation = invalidRange || luminance < legalBlack || luminance > legalWhite;
    const bool gamutViolation = invalidRange || r < channelMin || r > channelMax ||
        g < channelMin || g > channelMax || b < channelMin || b > channelMax;
    return {luminance, luminanceViolation, gamutViolation};
}

std::array<float, 3> ColorLuminance::clampBroadcastSafe(
    float r, float g, float b, float channelMin, float channelMax) {
    const auto clamp = [channelMin, channelMax](float value) {
        return std::clamp(std::isfinite(value) ? value : channelMin, channelMin, channelMax);
    };
    return {clamp(r), clamp(g), clamp(b)};
}

} // namespace ArtifactCore
