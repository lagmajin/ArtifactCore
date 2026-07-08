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
    BackIn,
    BackOut,
    BackInOut,
    BounceIn,
    BounceOut,
    BounceInOut,
    ElasticIn,
    ElasticOut,
    ElasticInOut,
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
    case EasingType::BackIn: return QStringLiteral("Back In");
    case EasingType::BackOut: return QStringLiteral("Back Out");
    case EasingType::BackInOut: return QStringLiteral("Back In-Out");
    case EasingType::BounceIn: return QStringLiteral("Bounce In");
    case EasingType::BounceOut: return QStringLiteral("Bounce Out");
    case EasingType::BounceInOut: return QStringLiteral("Bounce In-Out");
    case EasingType::ElasticIn: return QStringLiteral("Elastic In");
    case EasingType::ElasticOut: return QStringLiteral("Elastic Out");
    case EasingType::ElasticInOut: return QStringLiteral("Elastic In-Out");
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
    case EasingType::BackIn: {
        const float s = 1.70158f;
        return t * t * ((s + 1.0f) * t - s);
    }
    case EasingType::BackOut: {
        const float s = 1.70158f;
        const float u = t - 1.0f;
        return 1.0f + (u * u * ((s + 1.0f) * u + s));
    }
    case EasingType::BackInOut: {
        const float s = 1.70158f * 1.525f;
        if (t < 0.5f) {
            const float u = 2.0f * t;
            return 0.5f * (u * u * ((s + 1.0f) * u - s));
        }
        const float u = 2.0f * t - 2.0f;
        return 0.5f * (u * u * ((s + 1.0f) * u + s) + 2.0f);
    }
    case EasingType::BounceIn:
        return 1.0f - evaluateEasing(EasingType::BounceOut, 1.0f - t);
    case EasingType::BounceOut: {
        if (t < (1.0f / 2.75f)) {
            return 7.5625f * t * t;
        }
        if (t < (2.0f / 2.75f)) {
            const float u = t - (1.5f / 2.75f);
            return 7.5625f * u * u + 0.75f;
        }
        if (t < (2.5f / 2.75f)) {
            const float u = t - (2.25f / 2.75f);
            return 7.5625f * u * u + 0.9375f;
        }
        const float u = t - (2.625f / 2.75f);
        return 7.5625f * u * u + 0.984375f;
    }
    case EasingType::BounceInOut:
        if (t < 0.5f) {
            return 0.5f * (1.0f - evaluateEasing(EasingType::BounceOut, 1.0f - 2.0f * t));
        }
        return 0.5f * evaluateEasing(EasingType::BounceOut, 2.0f * t - 1.0f) + 0.5f;
    case EasingType::ElasticIn: {
        if (t <= 0.0f) {
            return 0.0f;
        }
        if (t >= 1.0f) {
            return 1.0f;
        }
        const float p = 0.3f;
        const float s = p / 4.0f;
        return -std::pow(2.0f, 10.0f * (t - 1.0f)) *
               std::sin((t - 1.0f - s) * (2.0f * 3.14159265f) / p);
    }
    case EasingType::ElasticOut: {
        if (t <= 0.0f) {
            return 0.0f;
        }
        if (t >= 1.0f) {
            return 1.0f;
        }
        const float p = 0.3f;
        const float s = p / 4.0f;
        return std::pow(2.0f, -10.0f * t) *
                   std::sin((t - s) * (2.0f * 3.14159265f) / p) +
               1.0f;
    }
    case EasingType::ElasticInOut: {
        if (t <= 0.0f) {
            return 0.0f;
        }
        if (t >= 1.0f) {
            return 1.0f;
        }
        const float p = 0.45f;
        const float s = p / 4.0f;
        const float x = t * 2.0f;
        if (x < 1.0f) {
            return -0.5f * std::pow(2.0f, 10.0f * (x - 1.0f)) *
                   std::sin((x - 1.0f - s) * (2.0f * 3.14159265f) / p);
        }
        return std::pow(2.0f, -10.0f * (x - 1.0f)) *
                   std::sin((x - 1.0f - s) * (2.0f * 3.14159265f) / p) *
                   0.5f + 1.0f;
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
    case EasingType::BounceIn:
        return static_cast<InterpolationType>(16);
    case EasingType::BounceOut:
        return static_cast<InterpolationType>(17);
    case EasingType::BounceInOut:
        return static_cast<InterpolationType>(18);
    case EasingType::ElasticIn:
        return static_cast<InterpolationType>(19);
    case EasingType::ElasticOut:
        return static_cast<InterpolationType>(20);
    case EasingType::ElasticInOut:
        return static_cast<InterpolationType>(21);
    case EasingType::BackIn:
        return static_cast<InterpolationType>(22);
    case EasingType::BackOut:
        return static_cast<InterpolationType>(23);
    case EasingType::BackInOut:
        return static_cast<InterpolationType>(24);
    case EasingType::Expo:
        return static_cast<InterpolationType>(11);
    case EasingType::Bezier:
        return static_cast<InterpolationType>(25);
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
        {QStringLiteral("Back In"), EasingType::BackIn},
        {QStringLiteral("Back Out"), EasingType::BackOut},
        {QStringLiteral("Back In-Out"), EasingType::BackInOut},
        {QStringLiteral("Bounce In"), EasingType::BounceIn},
        {QStringLiteral("Bounce Out"), EasingType::BounceOut},
        {QStringLiteral("Bounce In-Out"), EasingType::BounceInOut},
        {QStringLiteral("Elastic In"), EasingType::ElasticIn},
        {QStringLiteral("Elastic Out"), EasingType::ElasticOut},
        {QStringLiteral("Elastic In-Out"), EasingType::ElasticInOut},
        {QStringLiteral("Expo"), EasingType::Expo},
        {QStringLiteral("Bezier"), EasingType::Bezier},
    };
}

} // namespace ArtifactCore
