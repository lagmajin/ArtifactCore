module;

#include <cstring>
#include <type_traits>
#include <utility>

export module Core.ArtifactUtility;

export namespace ArtifactCore {

template<typename T>
constexpr std::remove_reference_t<T>&& artifactMove(T&& value) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(value);
}

template<typename T>
constexpr T&& artifactForward(std::remove_reference_t<T>& value) noexcept {
    return static_cast<T&&>(value);
}

template<typename T>
constexpr T&& artifactForward(std::remove_reference_t<T>&& value) noexcept {
    static_assert(!std::is_lvalue_reference_v<T>,
                  "artifactForward must not turn an rvalue into an lvalue");
    return static_cast<T&&>(value);
}

template<typename T, typename U = T>
constexpr T artifactExchange(T& obj, U&& newValue) noexcept {
    T old = artifactMove(obj);
    obj = artifactForward<U>(newValue);
    return old;
}

template<typename T, typename U>
constexpr bool artifactCmpEqual(T t, U u) noexcept {
    static_assert(std::is_integral_v<T> && std::is_integral_v<U>, "artifactCmpEqual requires integral types");
    if constexpr (std::is_signed_v<T> == std::is_signed_v<U>) {
        return t == u;
    } else if constexpr (std::is_signed_v<T>) {
        return t >= 0 && static_cast<std::make_unsigned_t<T>>(t) == u;
    } else {
        return u >= 0 && t == static_cast<std::make_unsigned_t<U>>(u);
    }
}

template<typename T, typename U>
constexpr bool artifactCmpLess(T t, U u) noexcept {
    static_assert(std::is_integral_v<T> && std::is_integral_v<U>, "artifactCmpLess requires integral types");
    if constexpr (std::is_signed_v<T> == std::is_signed_v<U>) {
        return t < u;
    } else if constexpr (std::is_signed_v<T>) {
        return t < 0 || static_cast<std::make_unsigned_t<T>>(t) < u;
    } else {
        return u >= 0 && t < static_cast<std::make_unsigned_t<U>>(u);
    }
}

template<typename T, typename U>
constexpr bool artifactCmpGreater(T t, U u) noexcept {
    return artifactCmpLess(u, t);
}

template<typename To, typename From>
constexpr To artifactBitCast(const From& from) noexcept {
    static_assert(sizeof(To) == sizeof(From), "artifactBitCast requires equal sizes");
    static_assert(std::is_trivially_copyable_v<To>, "artifactBitCast requires trivially copyable To");
    static_assert(std::is_trivially_copyable_v<From>, "artifactBitCast requires trivially copyable From");
    To to;
    std::memcpy(&to, &from, sizeof(To));
    return to;
}

// std::pair replacement.
template <typename First, typename Second>
struct Pair {
    First first{};
    Second second{};

    constexpr Pair() = default;
    constexpr Pair(const First& f, const Second& s) : first(f), second(s) {}

    [[nodiscard]] friend constexpr bool operator==(const Pair& a, const Pair& b)
        noexcept(noexcept(a.first == b.first) && noexcept(a.second == b.second))
    {
        return a.first == b.first && a.second == b.second;
    }

    [[nodiscard]] friend constexpr bool operator!=(const Pair& a, const Pair& b)
        noexcept(noexcept(a == b))
    {
        return !(a == b);
    }
};

template <typename First, typename Second>
[[nodiscard]] constexpr Pair<std::decay_t<First>, std::decay_t<Second>>
artifactMakePair(First&& first, Second&& second) noexcept(
    std::is_nothrow_constructible_v<Pair<std::decay_t<First>, std::decay_t<Second>>, First, Second>)
{
    return Pair<std::decay_t<First>, std::decay_t<Second>>(
        artifactForward<First>(first), artifactForward<Second>(second));
}

// boost::hash_combine style seed mixer for custom ArtifactHashMap keys.
inline std::size_t artifactHashCombine(std::size_t seed,
                                       std::size_t value) noexcept {
    constexpr std::size_t kGoldenRatio = 0x9e3779b97f4a7c15ULL;
    seed ^= value + kGoldenRatio + (seed << 6) + (seed >> 2);
    return seed;
}

} // namespace ArtifactCore
