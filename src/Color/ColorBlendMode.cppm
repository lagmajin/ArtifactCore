module;

#include <cmath>
#include <algorithm>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Color.BlendMode;




import Color.Float;
import Color.Conversion;
import Color.Luminance;

namespace ArtifactCore {

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

// -------------------------------------------------------------
// Main Blend Logic
// -------------------------------------------------------------
FloatColor ColorBlendMode::blend(const FloatColor& base, const FloatColor& blendColor, BlendMode mode, float opacity) {
    if (opacity <= 0.0f) return base;
    
    // RGBベースのブレンドモード処理
    auto applyBlend = [&](float (blendFunc)(float, float)) -> FloatColor {
        float outR = base.r() + opacity * (blendFunc(base.r(), blendColor.r()) - base.r());
        float outG = base.g() + opacity * (blendFunc(base.g(), blendColor.g()) - base.g());
        float outB = base.b() + opacity * (blendFunc(base.b(), blendColor.b()) - base.b());
        
        // Alpha補間: srcOverの基本ブレンド alpha = blendA + baseA*(1-blendA)
        // ここでのopacityは全体の影響度合い
        float outA = base.a(); // 今回はAlphaBlend計算を簡略化してRGB成分にフォーカス
        return FloatColor(outR, outG, outB, outA);
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
    
    // HSL Component Blend Modes
    case BlendMode::Hue:
    case BlendMode::Saturation:
    case BlendMode::Color:
    case BlendMode::Luminosity:
    {
        auto hsvBase = ColorConversion::RGBToHSV(base.r(), base.g(), base.b());
        auto hsvBlend = ColorConversion::RGBToHSV(blendColor.r(), blendColor.g(), blendColor.b());
        
        HSVColor resultHsv = hsvBase;
        
        if (mode == BlendMode::Hue || mode == BlendMode::Color) {
            resultHsv.h = hsvBlend.h;
        }
        if (mode == BlendMode::Saturation || mode == BlendMode::Color) {
            resultHsv.s = hsvBlend.s;
        }
        if (mode == BlendMode::Luminosity) {
            // 単純化のため輝度要素のみブレンドのVで置換
            // 本格的な輝度ブレンドアルゴリズムは Luma 変換を経由します
            resultHsv.v = hsvBlend.v;
        }
        
        auto rgb = ColorConversion::HSVToRGB(resultHsv);
        float outR = base.r() + opacity * (rgb[0] - base.r());
        float outG = base.g() + opacity * (rgb[1] - base.g());
        float outB = base.b() + opacity * (rgb[2] - base.b());
        return FloatColor(outR, outG, outB, base.a());
    }
    
    default:
        return base;
    }
}

} // namespace ArtifactCore
