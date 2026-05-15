module;
#include <algorithm>
#include <type_traits>

export module Utils.Numeric;

export namespace ArtifactCore {

template <class A, class B>
constexpr auto min_same(const A& a, const B& b)
{
    using T = std::common_type_t<std::decay_t<A>, std::decay_t<B>>;
    return std::min<T>(static_cast<T>(a), static_cast<T>(b));
}

template <class A, class B>
constexpr auto max_same(const A& a, const B& b)
{
    using T = std::common_type_t<std::decay_t<A>, std::decay_t<B>>;
    return std::max<T>(static_cast<T>(a), static_cast<T>(b));
}

} // namespace ArtifactCore
