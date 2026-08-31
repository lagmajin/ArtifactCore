module;

#include <limits>
#include <type_traits>

export module Core.ArtifactSaturation;

export namespace ArtifactCore {

template<typename T>
    requires std::is_integral_v<T>
constexpr T addSat(T a, T b) noexcept {
    if constexpr (std::is_unsigned_v<T>) {
        return a > std::numeric_limits<T>::max() - b
                   ? std::numeric_limits<T>::max() : static_cast<T>(a + b);
    } else {
        if (b > 0 && a > std::numeric_limits<T>::max() - b) return std::numeric_limits<T>::max();
        if (b < 0 && a < std::numeric_limits<T>::lowest() - b) return std::numeric_limits<T>::lowest();
        return static_cast<T>(a + b);
    }
}

template<typename T>
    requires std::is_integral_v<T>
constexpr T subSat(T a, T b) noexcept {
    if constexpr (std::is_unsigned_v<T>) {
        return a < b ? T{} : static_cast<T>(a - b);
    } else {
        if (b > 0 && a < std::numeric_limits<T>::lowest() + b) return std::numeric_limits<T>::lowest();
        if (b < 0 && a > std::numeric_limits<T>::max() + b) return std::numeric_limits<T>::max();
        return static_cast<T>(a - b);
    }
}

template<typename T>
    requires std::is_integral_v<T>
constexpr T mulSat(T a, T b) noexcept {
    if (a == 0 || b == 0) return T{};
    if constexpr (std::is_unsigned_v<T>) {
        return a > std::numeric_limits<T>::max() / b
                   ? std::numeric_limits<T>::max() : static_cast<T>(a * b);
    } else {
        constexpr T low = std::numeric_limits<T>::lowest();
        constexpr T high = std::numeric_limits<T>::max();
        if ((a == T{-1} && b == low) || (b == T{-1} && a == low)) return high;
        if (a > 0) {
            if (b > 0 && a > high / b) return high;
            if (b < 0 && b < low / a) return low;
        } else {
            if (b > 0 && a < low / b) return low;
            if (b < 0 && a < high / b) return high;
        }
        return static_cast<T>(a * b);
    }
}

template<typename To, typename From>
    requires (std::is_integral_v<To> && std::is_integral_v<From>)
constexpr To saturateCast(From value) noexcept {
    if constexpr (std::is_same_v<To, From>) {
        return value;
    } else if constexpr (std::is_signed_v<From> && std::is_unsigned_v<To>) {
        if (value <= 0) return To{};
        using UFrom = std::make_unsigned_t<From>;
        return static_cast<UFrom>(value) > std::numeric_limits<To>::max()
                   ? std::numeric_limits<To>::max() : static_cast<To>(value);
    } else if constexpr (std::is_unsigned_v<From> && std::is_signed_v<To>) {
        using UTo = std::make_unsigned_t<To>;
        return value > static_cast<UTo>(std::numeric_limits<To>::max())
                   ? std::numeric_limits<To>::max() : static_cast<To>(value);
    } else if constexpr (sizeof(To) >= sizeof(From)) {
        return static_cast<To>(value);
    } else {
        return value < static_cast<From>(std::numeric_limits<To>::lowest())
                   ? std::numeric_limits<To>::lowest()
                   : value > static_cast<From>(std::numeric_limits<To>::max())
                         ? std::numeric_limits<To>::max() : static_cast<To>(value);
    }
}

} // namespace ArtifactCore
