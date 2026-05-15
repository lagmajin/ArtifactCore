module;
#include <cmath>
#include <algorithm>
#include <numbers>
#include <QString>
#include <vector>

export module Animation.EasingCurveUtil;

import Math.Interpolate;

namespace ArtifactCore {

export struct EasingCandidate {
    EasingType type;
    QString name;
};

export inline float clampUnit(float t) {
    return std::clamp(t, 0.0f, 1.0f);
}

export QString easingTypeToString(EasingType type) {
    switch (type) {
        case EasingType::Linear: return "Linear";
        case EasingType::EaseInQuad: return "Ease In (Quad)";
        case EasingType::EaseOutQuad: return "Ease Out (Quad)";
        case EasingType::EaseInOutQuad: return "Ease In Out (Quad)";
        case EasingType::EaseInCubic: return "Ease In (Cubic)";
        case EasingType::EaseOutCubic: return "Ease Out (Cubic)";
        case EasingType::EaseInOutCubic: return "Ease In Out (Cubic)";
        case EasingType::EaseInSine: return "Ease In (Sine)";
        case EasingType::EaseOutSine: return "Ease Out (Sine)";
        case EasingType::EaseInOutSine: return "Ease In Out (Sine)";
        case EasingType::EaseInExpo: return "Ease In (Expo)";
        case EasingType::EaseOutExpo: return "Ease Out (Expo)";
        case EasingType::EaseInOutExpo: return "Ease In Out (Expo)";
        case EasingType::EaseInBack: return "Ease In (Back)";
        case EasingType::EaseOutBack: return "Ease Out (Back)";
        case EasingType::EaseInOutBack: return "Ease In Out (Back)";
        default: return "Unknown";
    }
}

export InterpolationType easingTypeToInterpolation(EasingType type) {
    switch (type) {
        case EasingType::Linear: return InterpolationType::Linear;
        case EasingType::EaseInQuad:
        case EasingType::EaseInCubic:
        case EasingType::EaseInSine:
        case EasingType::EaseInExpo:
            return InterpolationType::EaseIn;
        case EasingType::EaseOutQuad:
        case EasingType::EaseOutCubic:
        case EasingType::EaseOutSine:
        case EasingType::EaseOutExpo:
            return InterpolationType::EaseOut;
        case EasingType::EaseInOutQuad:
        case EasingType::EaseInOutCubic:
        case EasingType::EaseInOutSine:
        case EasingType::EaseInOutExpo:
            return InterpolationType::EaseInOut;
        case EasingType::EaseInBack: return InterpolationType::BackIn;
        case EasingType::EaseOutBack: return InterpolationType::BackOut;
        case EasingType::EaseInOutBack: return InterpolationType::BackInOut;
        default: return InterpolationType::Linear;
    }
}

export std::vector<EasingCandidate> defaultEasingCandidates() {
    return {
        { EasingType::Linear, "Linear" },
        { EasingType::EaseInQuad, "Ease In" },
        { EasingType::EaseOutQuad, "Ease Out" },
        { EasingType::EaseInOutQuad, "Ease In Out" },
        { EasingType::EaseOutBack, "Back" },
        { EasingType::EaseInOutExpo, "Expo" }
    };
}

export double evaluateEasing(EasingType type, double t) {
    t = std::clamp(t, 0.0, 1.0);
    
    switch (type) {
        case EasingType::Linear: return t;
        
        case EasingType::EaseInQuad: return t * t;
        case EasingType::EaseOutQuad: return t * (2.0 - t);
        case EasingType::EaseInOutQuad: return t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t;
        
        case EasingType::EaseInCubic: return t * t * t;
        case EasingType::EaseOutCubic: return (--t) * t * t + 1.0;
        case EasingType::EaseInOutCubic: return t < 0.5 ? 4.0 * t * t * t : (t - 1.0) * (2.0 * t - 2.0) * (2.0 * t - 2.0) + 1.0;
        
        case EasingType::EaseInSine: return 1.0 - std::cos(t * std::numbers::pi / 2.0);
        case EasingType::EaseOutSine: return std::sin(t * std::numbers::pi / 2.0);
        case EasingType::EaseInOutSine: return 0.5 * (1.0 - std::cos(std::numbers::pi * t));
        
        case EasingType::EaseInExpo: return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * (t - 1.0));
        case EasingType::EaseOutExpo: return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t);
        case EasingType::EaseInOutExpo: {
            if (t == 0.0) return 0.0;
            if (t == 1.0) return 1.0;
            if (t < 0.5) return 0.5 * std::pow(2.0, 10.0 * (2.0 * t - 1.0));
            return 0.5 * (2.0 - std::pow(2.0, -10.0 * (2.0 * t - 1.0)));
        }
        
        case EasingType::EaseInBack: {
            const double s = 1.70158;
            return t * t * ((s + 1.0) * t - s);
        }
        case EasingType::EaseOutBack: {
            const double s = 1.70158;
            return (--t) * t * ((s + 1.0) * t + s) + 1.0;
        }
        case EasingType::EaseInOutBack: {
            double s = 1.70158 * 1.525;
            if ((t *= 2.0) < 1.0) return 0.5 * (t * t * ((s + 1.0) * t - s));
            return 0.5 * ((t -= 2.0) * t * ((s + 1.0) * t + s) + 2.0);
        }

        default: return t;
    }
}

} // namespace ArtifactCore
