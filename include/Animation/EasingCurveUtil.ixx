module;

#include <algorithm>
#include <cmath>
#include <vector>

#include <QString>

export module Animation.EasingCurveUtil;

export namespace ArtifactCore {

enum class InterpolationType : int;

export enum class EasingType {
    Hold,
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Back,
    Expo,
    Bezier,
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
    case EasingType::Bezier: return QStringLiteral("Bezier");
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
    case EasingType::Bezier:
        {
            float x = t;
            for (int i = 0; i < 4; ++i) {
                const float x2 = x * x;
                const float mt = 1.0f - x;
                const float mt2 = mt * mt;
                const float dx = 3.0f * mt2 * 0.42f + 6.0f * mt * x * (0.58f - 0.42f) +
                                 3.0f * x2 * (1.0f - 0.58f);
                if (std::abs(dx) < 1e-6f) {
                    break;
                }
                const float cx = mt2 * mt + 3.0f * mt2 * x * 0.42f +
                                 3.0f * mt * x2 * 0.58f + x2 * x;
                x -= (cx - t) / dx;
            }
            const float mt = 1.0f - x;
            return 3.0f * mt * x * x + x * x * x;
        }
    }

    return t;
}

export inline InterpolationType easingTypeToInterpolation(EasingType type) noexcept
{
    switch (type) {
    case EasingType::Hold:
        return static_cast<InterpolationType>(1);
    case EasingType::Linear:
        return static_cast<InterpolationType>(0);
    case EasingType::EaseIn:
        return static_cast<InterpolationType>(3);
    case EasingType::EaseOut:
        return static_cast<InterpolationType>(4);
    case EasingType::EaseInOut:
        return static_cast<InterpolationType>(5);
    case EasingType::Back:
        return static_cast<InterpolationType>(22);
    case EasingType::Expo:
        return static_cast<InterpolationType>(10);
    case EasingType::Bezier:
        return static_cast<InterpolationType>(24);
    }
    return static_cast<InterpolationType>(0);
}

export inline std::vector<EasingCandidate> defaultEasingCandidates()
{
    return {
        {QStringLiteral("Hold"), EasingType::Hold},
        {QStringLiteral("Linear"), EasingType::Linear},
        {QStringLiteral("Ease In"), EasingType::EaseIn},
        {QStringLiteral("Ease Out"), EasingType::EaseOut},
        {QStringLiteral("Easy Ease"), EasingType::EaseInOut},
        {QStringLiteral("Back"), EasingType::Back},
        {QStringLiteral("Expo"), EasingType::Expo},
        {QStringLiteral("Bezier"), EasingType::Bezier},
    };
}

} // namespace ArtifactCore
