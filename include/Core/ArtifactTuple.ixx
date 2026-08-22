module;
#include <cstddef>
#include <type_traits>
#include <utility>

export module Core.ArtifactTuple;

import Core.ArtifactUtility;

export namespace ArtifactCore {

// Recursive std::tuple replacement. Element access is via artifactGet<I>.
template <typename... Ts>
class Tuple;

template <>
class Tuple<> {};

template <typename First, typename... Rest>
class Tuple<First, Rest...> : public Tuple<Rest...> {
public:
    constexpr Tuple() = default;

    constexpr explicit Tuple(const First& first, const Rest&... rest)
        : Tuple<Rest...>(rest...), head_(first) {}

    template <typename F2, typename... R2,
              typename = std::enable_if_t<
                  !std::is_same_v<std::decay_t<F2>, Tuple>>>
    constexpr explicit Tuple(F2&& first, R2&&... rest)
        : Tuple<Rest...>(artifactForward<R2>(rest)...),
          head_(artifactForward<F2>(first)) {}

    [[nodiscard]] constexpr First& head() noexcept { return head_; }
    [[nodiscard]] constexpr const First& head() const noexcept { return head_; }

    [[nodiscard]] friend constexpr bool operator==(const Tuple& a, const Tuple& b)
        noexcept(noexcept(a.head_ == b.head_))
    {
        return a.head_ == b.head_ &&
               static_cast<const Tuple<Rest...>&>(a) ==
                   static_cast<const Tuple<Rest...>&>(b);
    }

    [[nodiscard]] friend constexpr bool operator!=(const Tuple& a, const Tuple& b)
        noexcept(noexcept(a == b))
    {
        return !(a == b);
    }

private:
    First head_;
};

template <typename T>
struct TupleSize;

template <>
struct TupleSize<Tuple<>> {
    static constexpr std::size_t value = 0;
};

template <typename First, typename... Rest>
struct TupleSize<Tuple<First, Rest...>> {
    static constexpr std::size_t value = 1 + TupleSize<Tuple<Rest...>>::value;
};

template <typename T>
inline constexpr std::size_t tupleSizeV = TupleSize<T>::value;

namespace detail {
template <std::size_t Index, typename TupleType>
struct TupleGetter {
    static_assert(Index < tupleSizeV<TupleType>, "artifactGet index out of range");
};

template <typename First, typename... Rest>
struct TupleGetter<0, Tuple<First, Rest...>> {
    using TupleType = Tuple<First, Rest...>;
    using ElementType = First;
    [[nodiscard]] static constexpr First& get(TupleType& value) noexcept {
        return value.head();
    }
    [[nodiscard]] static constexpr const First& get(const TupleType& value) noexcept {
        return value.head();
    }
};

template <std::size_t Index, typename First, typename... Rest>
struct TupleGetter<Index, Tuple<First, Rest...>> {
    using Nested = TupleGetter<Index - 1, Tuple<Rest...>>;
    using TupleType = Tuple<First, Rest...>;
    using ElementType = typename Nested::ElementType;
    [[nodiscard]] static constexpr auto& get(TupleType& value) noexcept {
        return Nested::get(static_cast<Tuple<Rest...>&>(value));
    }
    [[nodiscard]] static constexpr const auto& get(const TupleType& value) noexcept {
        return Nested::get(static_cast<const Tuple<Rest...>&>(value));
    }
};
} // namespace detail

template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr auto& artifactGet(Tuple<Ts...>& value) noexcept {
    return detail::TupleGetter<Index, Tuple<Ts...>>::get(value);
}

template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr const auto& artifactGet(const Tuple<Ts...>& value) noexcept {
    return detail::TupleGetter<Index, Tuple<Ts...>>::get(value);
}

template <typename... Ts>
[[nodiscard]] constexpr Tuple<std::decay_t<Ts>...> artifactMakeTuple(Ts&&... values)
    noexcept(std::is_nothrow_constructible_v<Tuple<std::decay_t<Ts>...>, Ts...>)
{
    return Tuple<std::decay_t<Ts>...>(artifactForward<Ts>(values)...);
}

} // namespace ArtifactCore
