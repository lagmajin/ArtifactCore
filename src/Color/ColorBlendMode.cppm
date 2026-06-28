module;
#include <utility>

#include <cmath>
#include <algorithm>

module Color.BlendMode;

import Color.Float;
import Color.Conversion;
import Color.Luminance;

namespace ArtifactCore {

namespace {

inline float clamp01(const float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float outAlpha(const float srcAlpha, const float dstAlpha) {
    return srcAlpha + dstAlpha * (1.0f - srcAlpha);
}

inline FloatColor composeBlendResult(const FloatColor& base,
                                     const FloatColor& blendColor,
                                     const float srcAlpha,
                                     const FloatColor& blendedStraight) {
    const float dstAlpha = clamp01(base.a());
    const float outA = outAlpha(srcAlpha, dstAlpha);
    const float premulR =
        base.r() * dstAlpha * (1.0f - srcAlpha) +
        (blendedStraight.r() * dstAlpha + blendColor.r() * (1.0f - dstAlpha)) * srcAlpha;
    const float premulG =
        base.g() * dstAlpha * (1.0f - srcAlpha) +
        (blendedStraight.g() * dstAlpha + blendColor.g() * (1.0f - dstAlpha)) * srcAlpha;
    const float premulB =
        base.b() * dstAlpha * (1.0f - srcAlpha) +
        (blendedStraight.b() * dstAlpha + blendColor.b() * (1.0f - dstAlpha)) * srcAlpha;
    if (outA <= 1e-6f) {
        return FloatColor(0.0f, 0.0f, 0.0f, 0.0f);
    }
    return FloatColor(clamp01(premulR / outA),
                      clamp01(premulG / outA),
                      clamp01(premulB / outA),
                      outA);
}

inline FloatColor applyStencilLikeBlend(const FloatColor& base,
                                        const float factor) {
    const float clampedFactor = clamp01(factor);
    return FloatColor(base.r(), base.g(), base.b(), clamp01(base.a() * clampedFactor));
}

inline FloatColor applySilhouetteLikeBlend(const FloatColor& base,
                                           const float factor) {
    const float clampedFactor = clamp01(1.0f - factor);
    return FloatColor(base.r(), base.g(), base.b(), clamp01(base.a() * clampedFactor));
}

} // namespace

// --- Helper macros/inline functions ---
inline float ColorBlendMode::blendAdd(float b, float f) {
    return std::min(b + f, 1.0f);
}
inline float ColorBlendMode::blendSubtract(float b, float f) {
    return std::max(b - f, 0.0f);
}
inline float ColorBlendMode::blendMultiply(float b, float f) {
    return b * f;
}
inline float ColorBlendMode::blendScreen(float b, float f) {
    return 1.0f - (1.0f - b) * (1.0f - f);
}
inline float ColorBlendMode::blendOverlay(float b, float f) {
    return (b < 0.5f) ? (2.0f * b * f) : (1.0f - 2.0f * (1.0f - b) * (1.0f - f));
}
inline float ColorBlendMode::blendDarken(float b, float f) {
    return std::min(b, f);
}
inline float ColorBlendMode::blendLighten(float b, float f) {
    return std::max(b, f);
}
inline float ColorBlendMode::blendColorDodge(float b, float f) {
    if (f == 1.0f) return 1.0f;
    return std::min(1.0f, b / (1.0f - f));
}
inline float ColorBlendMode::blendColorBurn(float b, float f) {
    if (f == 0.0f) return 0.0f;
    return std::max(0.0f, 1.0f - (1.0f - b) / f);
}
inline float ColorBlendMode::blendHardLight(float b, float f) {
    return blendOverlay(f, b);
}
inline float ColorBlendMode::blendSoftLight(float b, float f) {
    if (f < 0.5f) {
        return b - (1.0f - 2.0f * f) * b * (1.0f - b);
    } else {
        float d = (b <= 0.25f) ? ((16.0f * b - 12.0f) * b + 4.0f) * b : std::sqrt(b);
        return b + (2.0f * f - 1.0f) * (d - b);
    }
}
inline float ColorBlendMode::blendDifference(float b, float f) {
    return std::abs(b - f);
}
inline float ColorBlendMode::blendExclusion(float b, float f) {
    return b + f - 2.0f * b * f;
}
inline float ColorBlendMode::blendLinearBurn(float b, float f) {
    return std::max(b + f - 1.0f, 0.0f);
}
inline float ColorBlendMode::blendDivide(float b, float f) {
    return std::min(b / std::max(f, 1e-6f), 1.0f);
}
inline float ColorBlendMode::blendPinLight(float b, float f) {
    return (f < 0.5f) ? std::min(b, 2.0f * f) : std::max(b, 2.0f * (f - 0.5f));
}
inline float ColorBlendMode::blendVividLight(float b, float f) {
    return (f < 0.5f)
        ? (f == 0.0f ? 0.0f : std::max(1.0f - (1.0f - b) / (2.0f * f), 0.0f))
        : (f == 1.0f ? 1.0f : std::min(b / (2.0f * (1.0f - f)), 1.0f));
}
inline float ColorBlendMode::blendLinearLight(float b, float f) {
    return std::clamp(b + 2.0f * f - 1.0f, 0.0f, 1.0f);
}
inline float ColorBlendMode::blendHardMix(float b, float f) {
    return (b + f >= 1.0f) ? 1.0f : 0.0f;
}

// -------------------------------------------------------------
// Main Blend Logic
// -------------------------------------------------------------
FloatColor ColorBlendMode::blend(const FloatColor& base, const FloatColor& blendColor, BlendMode mode, float opacity) {
    const float srcAlpha = clamp01(opacity);
    if (srcAlpha <= 0.0f) return base;

    auto applyBlend = [&](float (blendFunc)(float, float)) -> FloatColor {
        const FloatColor blendedStraight(
            clamp01(blendFunc(base.r(), blendColor.r())),
            clamp01(blendFunc(base.g(), blendColor.g())),
            clamp01(blendFunc(base.b(), blendColor.b())),
            1.0f);
        return composeBlendResult(base, blendColor, srcAlpha, blendedStraight);
    };

    switch (mode) {
    case BlendMode::Normal:       return applyBlend([](float, float f) { return f; });
    case BlendMode::Add:          return applyBlend(blendAdd);
    case BlendMode::Subtract:     return applyBlend(blendSubtract);
    case BlendMode::Multiply:     return applyBlend(blendMultiply);
    case BlendMode::Screen:       return applyBlend(blendScreen);
    case BlendMode::Overlay:      return applyBlend(blendOverlay);
    case BlendMode::Darken:       return applyBlend(blendDarken);
    case BlendMode::Lighten:      return applyBlend(blendLighten);
    case BlendMode::ColorDodge:   return applyBlend(blendColorDodge);
    case BlendMode::ColorBurn:    return applyBlend(blendColorBurn);
    case BlendMode::HardLight:    return applyBlend(blendHardLight);
    case BlendMode::SoftLight:    return applyBlend(blendSoftLight);
    case BlendMode::Difference:   return applyBlend(blendDifference);
    case BlendMode::Exclusion:    return applyBlend(blendExclusion);
    case BlendMode::LinearBurn:   return applyBlend(blendLinearBurn);
    case BlendMode::Divide:       return applyBlend(blendDivide);
    case BlendMode::PinLight:     return applyBlend(blendPinLight);
    case BlendMode::VividLight:   return applyBlend(blendVividLight);
    case BlendMode::LinearLight:  return applyBlend(blendLinearLight);
    case BlendMode::HardMix:      return applyBlend(blendHardMix);
    case BlendMode::ClassicColorBurn: return applyBlend(blendColorBurn);
    case BlendMode::LinearDodge:      return applyBlend(blendAdd);
    case BlendMode::ClassicColorDodge:return applyBlend(blendColorDodge);
    case BlendMode::ClassicDifference:return applyBlend(blendDifference);
    
    case BlendMode::Hue:
    case BlendMode::Saturation:
    case BlendMode::Color:
    case BlendMode::Luminosity:
    {
        HSLColor baseHsl = ColorConversion::RGBToHSL(base.r(), base.g(), base.b());
        HSLColor blendHsl = ColorConversion::RGBToHSL(blendColor.r(), blendColor.g(), blendColor.b());
        HSLColor resultHsl = baseHsl;

        if (mode == BlendMode::Hue || mode == BlendMode::Color) {
            resultHsl.h = blendHsl.h;
        }
        if (mode == BlendMode::Saturation || mode == BlendMode::Color) {
            resultHsl.s = blendHsl.s;
        }
        if (mode == BlendMode::Luminosity) {
            resultHsl.l = blendHsl.l;
        }
        const auto rgb = ColorConversion::HSLToRGB(resultHsl);
        return composeBlendResult(
            base, blendColor, srcAlpha,
            FloatColor(clamp01(rgb[0]), clamp01(rgb[1]), clamp01(rgb[2]), 1.0f));
    }

    case BlendMode::Dissolve:
    case BlendMode::DancingDissolve:
        return composeBlendResult(base, blendColor, srcAlpha, blendColor);
    case BlendMode::StencilAlpha:
        return applyStencilLikeBlend(base, srcAlpha);
    case BlendMode::StencilLuma:
        return applyStencilLikeBlend(
            base, clamp01(ColorLuminance::calculate(blendColor.r(), blendColor.g(),
                                                    blendColor.b()) *
                          srcAlpha));
    case BlendMode::SilhouetteAlpha:
        return applySilhouetteLikeBlend(base, srcAlpha);
    case BlendMode::SilhouetteLuma:
        return applySilhouetteLikeBlend(
            base, clamp01(ColorLuminance::calculate(blendColor.r(), blendColor.g(),
                                                    blendColor.b()) *
                          srcAlpha));

    default:
        return base;
    }
}

} // namespace ArtifactCore
