module;
#include <optional>
#include <utility>

export module Utils.Optional;

import Utils.Result;

export namespace ArtifactCore {

template <typename T>
using Optional = std::optional<T>;

template <typename T>
inline constexpr bool HasValue(const Optional<T>& value) noexcept
{
 return value.has_value();
}

template <typename T>
inline T ValueOr(const Optional<T>& value, T fallback)
{
 return value.has_value() ? *value : std::move(fallback);
}

template <typename T, typename F>
inline T ValueOrElse(const Optional<T>& value, F&& fallbackFactory)
{
 return value.has_value() ? *value : static_cast<T>(fallbackFactory());
}

template <typename T, typename F>
inline auto Transform(const Optional<T>& value, F&& fn)
    -> Optional<std::decay_t<decltype(fn(std::declval<const T&>()))>>
{
 using U = std::decay_t<decltype(fn(std::declval<const T&>()))>;
 if (!value.has_value()) {
  return {};
 }
 return Optional<U>(fn(*value));
}

template <typename T, typename F>
inline auto AndThen(const Optional<T>& value, F&& fn)
    -> decltype(fn(std::declval<const T&>()))
{
 using R = decltype(fn(std::declval<const T&>()));
 if (!value.has_value()) {
  return R{};
 }
 return fn(*value);
}

template <typename T, typename F, typename U>
inline U TransformOrElse(const Optional<T>& value, F&& fn, U fallback)
{
 return value.has_value() ? static_cast<U>(fn(*value)) : std::move(fallback);
}

template <typename T>
inline Result<T> ToResult(const Optional<T>& value, ErrorCode code = ErrorCode::Failed)
{
 if (!value.has_value()) {
  return Result<T>(Status::fail(code));
 }
 return Result<T>(*value);
}

template <typename T>
inline Result<T> OrElseResult(const Optional<T>& value, ErrorCode code = ErrorCode::Failed)
{
 return ToResult(value, code);
}

template <typename T, typename Pred>
inline Optional<T> Filter(const Optional<T>& value, Pred&& pred)
{
 if (!value.has_value() || !pred(*value)) {
  return {};
 }
 return value;
}

template <typename T>
inline T UnwrapOrDefault(const Optional<T>& value)
{
 return value.has_value() ? *value : T{};
}

template <typename T>
inline bool HasValueOr(const Optional<T>& value, bool fallback)
{
 return value.has_value() ? true : fallback;
}

template <typename T>
inline bool IsPresent(const Optional<T>& value)
{
 return value.has_value();
}

template <typename T>
inline const T* ValuePtr(const Optional<T>& value) noexcept
{
 return value.has_value() ? &*value : nullptr;
}

template <typename T>
inline T* ValuePtr(Optional<T>& value) noexcept
{
 return value.has_value() ? &*value : nullptr;
}

template <typename T>
inline bool HasValuePtr(const Optional<T>& value) noexcept
{
 return ValuePtr(value) != nullptr;
}

template <typename T>
inline bool IsEmpty(const Optional<T>& value) noexcept
{
 return !value.has_value();
}

template <typename T>
inline const T* GetValuePtr(const Optional<T>& value) noexcept
{
 return ValuePtr(value);
}

template <typename T>
inline T ValueOrDefault(const Optional<T>& value)
{
 return UnwrapOrDefault(value);
}

template <typename T>
inline bool HasAnyValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline const T* Ptr(const Optional<T>& value) noexcept
{
 return ValuePtr(value);
}

template <typename T>
inline bool HasAny(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline const T* ToPtr(const Optional<T>& value) noexcept
{
 return ValuePtr(value);
}

template <typename T>
inline bool HasContent(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool ContainsValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPopulated(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasData(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool NotEmpty(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasElements(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasEntries(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Some(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasSome(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Any(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Exists(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Present(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Available(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool ExistsValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasPresence(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasBeing(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsThere(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasExistence(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsExisting(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasBeingValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsAlive(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentBeing(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasValueState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool ExistsState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool StateExists(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasStoredValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsStored(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasContainedValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsHeld(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKept(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKept(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptValueState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKeptValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKeptState(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptPresence(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKeptPresence(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptStateValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsStoredValue(const Optional<T>& value) noexcept
{
 return HasValue(value);
}

}
