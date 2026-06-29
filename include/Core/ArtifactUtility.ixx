module;

#include <cstring>
#include <type_traits>
#include <utility>

export module Core.ArtifactUtility;

export namespace ArtifactCore {

template<typename T, typename U = T>
constexpr T artifactExchange(T& obj, U&& newValue) noexcept {
    T old = std::move(obj);
    obj = std::forward<U>(newValue);
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

} // namespace ArtifactCore
