module;

#include <algorithm>
#include <cmath>
#include <vector>

#include <QString>

export module Animation.EasingCurveUtil;

import Math.Interpolate;

export namespace ArtifactCore {

export enum class EasingType {
    Hold,
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Back,
    Expo,
};

export struct EasingCandidate {
    QString name;
    EasingType type = EasingType::Linear;
};

export inline float clampUnit(float t) noexcept
{
    return std::clamp(t, 0.0f, 1.0f);
}

export inline QString easingTypeToString(EasingType type)
{
    switch (type) {
    case EasingType::Hold: return QStringLiteral("Hold");
    case EasingType::Linear: return QStringLiteral("Linear");
    case EasingType::EaseIn: return QStringLiteral("Ease In");
    case EasingType::EaseOut: return QStringLiteral("Ease Out");
    case EasingType::EaseInOut: return QStringLiteral("Ease In-Out");
    case EasingType::Back: return QStringLiteral("Back");
    case EasingType::Expo: return QStringLiteral("Expo");
    }
    return QStringLiteral("Linear");
}

export inline float evaluateEasing(EasingType type, float t) noexcept
{
    t = clampUnit(t);

    switch (type) {
    case EasingType::Hold:
        return t < 1.0f ? 0.0f : 1.0f;
    case EasingType::Linear:
        return t;
    case EasingType::EaseIn:
        return t * t;
    case EasingType::EaseOut: {
        const float u = 1.0f - t;
        return 1.0f - (u * u);
    }
    case EasingType::EaseInOut:
        if (t < 0.5f) {
            return 2.0f * t * t;
        }
        return 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    case EasingType::Back: {
        const float s = 1.70158f;
        const float u = t - 1.0f;
        return 1.0f + (u * u * ((s + 1.0f) * u + s));
    }
    case EasingType::Expo:
        if (t <= 0.0f) {
            return 0.0f;
        }
        if (t >= 1.0f) {
            return 1.0f;
        }
        return std::pow(2.0f, 10.0f * (t - 1.0f));
    }

    return t;
}

export inline InterpolationType easingTypeToInterpolation(EasingType type) noexcept
{
    switch (type) {
    case EasingType::Hold:
        return InterpolationType::Constant;
    case EasingType::Linear:
        return InterpolationType::Linear;
    case EasingType::EaseIn:
        return InterpolationType::EaseIn;
    case EasingType::EaseOut:
        return InterpolationType::EaseOut;
    case EasingType::EaseInOut:
        return InterpolationType::EaseInOut;
    case EasingType::Back:
        return InterpolationType::BackOut;
    case EasingType::Expo:
        return InterpolationType::Exponential;
    }
    return InterpolationType::Linear;
}

export inline std::vector<EasingCandidate> defaultEasingCandidates()
{
    return {
        {QStringLiteral("Hold"), EasingType::Hold},
        {QStringLiteral("Linear"), EasingType::Linear},
        {QStringLiteral("Ease In"), EasingType::EaseIn},
        {QStringLiteral("Ease Out"), EasingType::EaseOut},
        {QStringLiteral("Ease In-Out"), EasingType::EaseInOut},
        {QStringLiteral("Back"), EasingType::Back},
        {QStringLiteral("Expo"), EasingType::Expo},
    };
}

} // namespace ArtifactCore
