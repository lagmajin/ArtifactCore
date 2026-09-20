module;
#include <cmath>
#include <limits>
#include <type_traits>

export module Core.ArtifactMath;

export namespace ArtifactCore {

// std::numeric_limits replacement for arithmetic types.
template <typename T>
struct NumericTraits {
    static constexpr bool isInteger = std::is_integral_v<T>;
    static constexpr bool isSigned = std::is_signed_v<T>;

    static constexpr T maxValue() noexcept { return std::numeric_limits<T>::max(); }
    static constexpr T minValue() noexcept { return std::numeric_limits<T>::lowest(); }
};

// Self-contained numeric / algorithm primitives replacing the most common
// <cmath> / <algorithm> usages so module code can avoid `import std`.
// Floating-point overloads live beside integral ones; mixed-argument calls
// promote through the usual arithmetic conversions.

template <typename T>
constexpr const T& artifactMax(const T& a, const T& b) noexcept {
    return a < b ? b : a;
}

template <typename T>
constexpr const T& artifactMin(const T& a, const T& b) noexcept {
    return b < a ? b : a;
}

template <typename T, typename... Rest>
constexpr const T& artifactMax(const T& a, const T& b, const Rest&... rest) noexcept {
    if constexpr (sizeof...(rest) == 0) {
        return artifactMax(a, b);
    } else {
        return artifactMax(artifactMax(a, b), rest...);
    }
}

template <typename T, typename... Rest>
constexpr const T& artifactMin(const T& a, const T& b, const Rest&... rest) noexcept {
    if constexpr (sizeof...(rest) == 0) {
        return artifactMin(a, b);
    } else {
        return artifactMin(artifactMin(a, b), rest...);
    }
}

template <typename T>
constexpr const T& artifactClamp(const T& value, const T& low, const T& high) noexcept {
    return value < low ? low : (high < value ? high : value);
}

template <typename T>
constexpr T artifactAbs(const T& value) noexcept {
    return value < T{} ? -value : value;
}

inline bool artifactIsFinite(const float value) noexcept { return std::isfinite(value); }
inline bool artifactIsFinite(const double value) noexcept { return std::isfinite(value); }
inline bool artifactIsFinite(const long double value) noexcept { return std::isfinite(value); }

inline bool artifactIsNaN(const float value) noexcept { return std::isnan(value); }
inline bool artifactIsNaN(const double value) noexcept { return std::isnan(value); }

inline float artifactSqrt(const float value) noexcept { return std::sqrt(value); }
inline double artifactSqrt(const double value) noexcept { return std::sqrt(value); }

inline float artifactHypot(const float x, const float y) noexcept {
    return std::hypot(x, y);
}
inline double artifactHypot(const double x, const double y) noexcept {
    return std::hypot(x, y);
}

template <typename T>
constexpr void artifactSwap(T& a, T& b) noexcept {
    T temporary = static_cast<T&&>(a);
    a = static_cast<T&&>(b);
    b = static_cast<T&&>(temporary);
}

inline float artifactPow(const float base, const float exponent) noexcept {
    return std::pow(base, exponent);
}
inline double artifactPow(const double base, const double exponent) noexcept {
    return std::pow(base, exponent);
}

inline float artifactSin(const float value) noexcept { return std::sin(value); }
inline double artifactSin(const double value) noexcept { return std::sin(value); }
inline float artifactCos(const float value) noexcept { return std::cos(value); }
inline double artifactCos(const double value) noexcept { return std::cos(value); }
inline float artifactTan(const float value) noexcept { return std::tan(value); }
inline double artifactTan(const double value) noexcept { return std::tan(value); }

inline float artifactAtan2(const float y, const float x) noexcept {
    return std::atan2(y, x);
}
inline double artifactAtan2(const double y, const double x) noexcept {
    return std::atan2(y, x);
}

inline float artifactFloor(const float value) noexcept { return std::floor(value); }
inline double artifactFloor(const double value) noexcept { return std::floor(value); }
inline float artifactCeil(const float value) noexcept { return std::ceil(value); }
inline double artifactCeil(const double value) noexcept { return std::ceil(value); }

inline float artifactRound(const float value) noexcept { return std::round(value); }
inline double artifactRound(const double value) noexcept { return std::round(value); }

inline float artifactAtan(const float value) noexcept { return std::atan(value); }
inline double artifactAtan(const double value) noexcept { return std::atan(value); }

inline float artifactAcos(const float value) noexcept { return std::acos(value); }
inline double artifactAcos(const double value) noexcept { return std::acos(value); }

inline float artifactFmod(const float x, const float y) noexcept { return std::fmod(x, y); }
inline double artifactFmod(const double x, const double y) noexcept { return std::fmod(x, y); }

// Arithmetic fallbacks mirroring std's arithmetic overloads (integral args
// promote to double). Exact float/double overloads above stay preferred.
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactSin(const T value) noexcept { return std::sin(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactCos(const T value) noexcept { return std::cos(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactTan(const T value) noexcept { return std::tan(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactAtan(const T value) noexcept { return std::atan(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactAcos(const T value) noexcept { return std::acos(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactSqrt(const T value) noexcept { return std::sqrt(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactFloor(const T value) noexcept { return std::floor(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactCeil(const T value) noexcept { return std::ceil(static_cast<double>(value)); }
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline double artifactRound(const T value) noexcept { return std::round(static_cast<double>(value)); }
template <typename X, typename Y,
          std::enable_if_t<std::is_arithmetic_v<X> && std::is_arithmetic_v<Y>, int> = 0>
inline double artifactPow(const X base, const Y exponent) noexcept {
    return std::pow(static_cast<double>(base), static_cast<double>(exponent));
}
template <typename X, typename Y,
          std::enable_if_t<std::is_arithmetic_v<X> && std::is_arithmetic_v<Y>, int> = 0>
inline double artifactAtan2(const X y, const Y x) noexcept {
    return std::atan2(static_cast<double>(y), static_cast<double>(x));
}
template <typename X, typename Y,
          std::enable_if_t<std::is_arithmetic_v<X> && std::is_arithmetic_v<Y>, int> = 0>
inline double artifactFmod(const X x, const Y y) noexcept {
    return std::fmod(static_cast<double>(x), static_cast<double>(y));
}
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline bool artifactIsFinite(const T) noexcept { return true; }
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline bool artifactIsNaN(const T) noexcept { return false; }

inline long artifactLround(const float value) noexcept { return std::lround(value); }
inline long artifactLround(const double value) noexcept { return std::lround(value); }
inline long long artifactLlround(const float value) noexcept { return std::llround(value); }
inline long long artifactLlround(const double value) noexcept { return std::llround(value); }

// Linear interpolation matching KeyframeInterpolator expectations.
template <typename T, typename U>
constexpr auto artifactLerp(const T& a, const T& b, const U& t) noexcept {
    return a + (b - a) * t;
}

} // namespace ArtifactCore
