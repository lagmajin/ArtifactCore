module;

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

export module Math.ArtifactMinMax;

export namespace ArtifactCore {

template<typename T>
constexpr T artifactMin(const T& a, const T& b) noexcept {
    static_assert(std::is_arithmetic_v<T>, "artifactMin requires arithmetic type");
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(a)) return b;
        if (std::isnan(b)) return a;
        return a < b ? a : b;
    } else {
        return std::min(a, b);
    }
}

template<typename T>
constexpr T artifactMax(const T& a, const T& b) noexcept {
    static_assert(std::is_arithmetic_v<T>, "artifactMax requires arithmetic type");
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(a)) return b;
        if (std::isnan(b)) return a;
        if (std::isinf(a) && std::signbit(a)) return b;
        if (std::isinf(b) && std::signbit(b)) return a;
        return a > b ? a : b;
    } else {
        return std::max(a, b);
    }
}

template<typename T, typename U>
constexpr auto artifactMin(const T& a, const U& b) noexcept -> decltype(a + b) {
    using Common = std::common_type_t<T, U>;
    const Common ca = static_cast<Common>(a);
    const Common cb = static_cast<Common>(b);
    return artifactMin(ca, cb);
}

template<typename T, typename U>
constexpr auto artifactMax(const T& a, const U& b) noexcept -> decltype(a + b) {
    using Common = std::common_type_t<T, U>;
    const Common ca = static_cast<Common>(a);
    const Common cb = static_cast<Common>(b);
    return artifactMax(ca, cb);
}

struct MinMaxResult {
    double min;
    double max;
};

inline MinMaxResult safeNormalizeRange(double value, double minVal, double maxVal) noexcept {
    if (std::isnan(value) || std::isnan(minVal) || std::isnan(maxVal)) {
        return MinMaxResult{0.0, 1.0};
    }

    if (std::fabs(maxVal - minVal) < std::numeric_limits<double>::epsilon()) {
        return MinMaxResult{value, value};
    }

    const double safeMin = artifactMin(minVal, maxVal);
    const double safeMax = artifactMax(minVal, maxVal);

    return MinMaxResult{safeMin, safeMax};
}

template<typename T>
constexpr T clamp(T value, T minVal, T maxVal) noexcept {
    return artifactMax(minVal, artifactMin(value, maxVal));
}

template<typename T, typename U>
constexpr T clamp(T value, U minVal, U maxVal) noexcept {
    return artifactMax(static_cast<T>(minVal), artifactMin(value, static_cast<T>(maxVal)));
}

template<typename T>
constexpr T artifactLerp(T a, T b, T t) noexcept {
    static_assert(std::is_arithmetic_v<T>, "artifactLerp requires arithmetic type");
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(a)) return b;
        if (std::isnan(b)) return a;
        if (std::isnan(t)) return a;
    }
    return a + t * (b - a);
}

template<typename T, typename U>
constexpr auto artifactLerp(T a, T b, U t) noexcept -> decltype(a + b) {
    using Common = std::common_type_t<T, U>;
    return artifactLerp(static_cast<Common>(a), static_cast<Common>(b), static_cast<Common>(t));
}

} // namespace ArtifactCore
