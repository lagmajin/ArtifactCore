module;
#include <algorithm>
#include <cmath>
#include <optional>

#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
export module Color.Bridge;

import Color.Float;
import FloatRGBA;

export namespace ArtifactCore {

// Canonical Qt/JSON boundary for the float color types. Layer and effect
// code previously re-implemented these conversions per file with slightly
// different rounding and alpha defaults; use these instead.

namespace detail {
inline float clampedChannel(const double value) noexcept
{
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}
} // namespace detail

[[nodiscard]] inline QColor toQColor(const FloatColor& color) noexcept
{
    return QColor::fromRgbF(detail::clampedChannel(color.r()),
                            detail::clampedChannel(color.g()),
                            detail::clampedChannel(color.b()),
                            detail::clampedChannel(color.a()));
}

[[nodiscard]] inline QColor toQColor(const FloatRGBA& color) noexcept
{
    return QColor::fromRgbF(detail::clampedChannel(color.r()),
                            detail::clampedChannel(color.g()),
                            detail::clampedChannel(color.b()),
                            detail::clampedChannel(color.a()));
}

[[nodiscard]] inline FloatColor toFloatColor(const QColor& color) noexcept
{
    if (!color.isValid()) {
        return FloatColor{};
    }
    return FloatColor(static_cast<float>(color.redF()),
                      static_cast<float>(color.greenF()),
                      static_cast<float>(color.blueF()),
                      static_cast<float>(color.alphaF()));
}

[[nodiscard]] inline FloatRGBA toFloatRGBA(const QColor& color) noexcept
{
    if (!color.isValid()) {
        return FloatRGBA(0.0f, 0.0f, 0.0f, 1.0f);
    }
    return FloatRGBA(static_cast<float>(color.redF()),
                     static_cast<float>(color.greenF()),
                     static_cast<float>(color.blueF()),
                     static_cast<float>(color.alphaF()));
}

[[nodiscard]] inline QString colorToHexArgb(const FloatColor& color) noexcept
{
    return toQColor(color).name(QColor::HexArgb);
}

// Canonical JSON shape: {"r","g","b","a"} as 0..1 floats.
[[nodiscard]] inline QJsonObject colorToJson(const FloatColor& color) noexcept
{
    QJsonObject object;
    object.insert(QStringLiteral("r"), static_cast<double>(color.r()));
    object.insert(QStringLiteral("g"), static_cast<double>(color.g()));
    object.insert(QStringLiteral("b"), static_cast<double>(color.b()));
    object.insert(QStringLiteral("a"), static_cast<double>(color.a()));
    return object;
}

[[nodiscard]] inline QJsonObject colorToJson(const FloatRGBA& color) noexcept
{
    QJsonObject object;
    object.insert(QStringLiteral("r"), static_cast<double>(color.r()));
    object.insert(QStringLiteral("g"), static_cast<double>(color.g()));
    object.insert(QStringLiteral("b"), static_cast<double>(color.b()));
    object.insert(QStringLiteral("a"), static_cast<double>(color.a()));
    return object;
}

namespace detail {
inline std::optional<FloatColor> floatColorFromObject(const QJsonObject& object) noexcept
{
    if (!object.contains(QStringLiteral("r")) ||
        !object.contains(QStringLiteral("g")) ||
        !object.contains(QStringLiteral("b"))) {
        return std::nullopt;
    }
    const double r = object.value(QStringLiteral("r")).toDouble();
    const double g = object.value(QStringLiteral("g")).toDouble();
    const double b = object.value(QStringLiteral("b")).toDouble();
    const double a = object.value(QStringLiteral("a")).toDouble(1.0);
    if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) ||
        !std::isfinite(a)) {
        return std::nullopt;
    }
    return FloatColor(static_cast<float>(r), static_cast<float>(g),
                      static_cast<float>(b), static_cast<float>(a));
}

inline std::optional<FloatColor> floatColorFromString(const QString& text) noexcept
{
    const QColor color(text.trimmed());
    if (!color.isValid()) {
        return std::nullopt;
    }
    return toFloatColor(color);
}
} // namespace detail

// Accepts {"r","g","b","a"} objects and "#RRGGBB[AA]" strings. Returns
// `fallback` for anything else so callers never observe invalid colors.
[[nodiscard]] inline FloatColor floatColorFromJson(
    const QJsonValue& value, const FloatColor& fallback = FloatColor{}) noexcept
{
    if (value.isObject()) {
        const auto parsed = detail::floatColorFromObject(value.toObject());
        return parsed ? *parsed : fallback;
    }
    if (value.isString()) {
        const auto parsed = detail::floatColorFromString(value.toString());
        return parsed ? *parsed : fallback;
    }
    return fallback;
}

[[nodiscard]] inline FloatRGBA floatRgbaFromJson(
    const QJsonValue& value, const FloatRGBA& fallback = FloatRGBA(0.0f, 0.0f, 0.0f, 1.0f)) noexcept
{
    if (value.isObject()) {
        const auto parsed = detail::floatColorFromObject(value.toObject());
        if (parsed) {
            return FloatRGBA(parsed->r(), parsed->g(), parsed->b(), parsed->a());
        }
        return fallback;
    }
    if (value.isString()) {
        const auto parsed = detail::floatColorFromString(value.toString());
        if (parsed) {
            return FloatRGBA(parsed->r(), parsed->g(), parsed->b(), parsed->a());
        }
    }
    return fallback;
}

} // namespace ArtifactCore
