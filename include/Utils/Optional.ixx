module;
#include <optional>
#include <utility>

export module Utils.Optional;

import Utils.Result;

export namespace ArtifactCore {

template <typename T>
using Optional = std::optional<T>;

template <typename T>
inline constexpr bool HasValue(const std::optional<T>& value) noexcept
{
 return value.has_value();
}

template <typename T>
inline T ValueOr(const std::optional<T>& value, T fallback)
{
 return value.has_value() ? *value : std::move(fallback);
}

template <typename T, typename F>
inline T ValueOrElse(const std::optional<T>& value, F&& fallbackFactory)
{
 return value.has_value() ? *value : static_cast<T>(fallbackFactory());
}

template <typename T, typename F>
inline auto Transform(const std::optional<T>& value, F&& fn)
    -> std::optional<std::decay_t<decltype(fn(std::declval<const T&>()))>>
{
 using U = std::decay_t<decltype(fn(std::declval<const T&>()))>;
 if (!value.has_value()) {
  return std::nullopt;
 }
 return std::optional<U>(fn(*value));
}

template <typename T, typename F>
inline auto AndThen(const std::optional<T>& value, F&& fn)
    -> decltype(fn(std::declval<const T&>()))
{
 using R = decltype(fn(std::declval<const T&>()));
 if (!value.has_value()) {
  return R{};
 }
 return fn(*value);
}

template <typename T, typename F, typename U>
inline U TransformOrElse(const std::optional<T>& value, F&& fn, U fallback)
{
 return value.has_value() ? static_cast<U>(fn(*value)) : std::move(fallback);
}

template <typename T>
inline Result<T> ToResult(const std::optional<T>& value, ErrorCode code = ErrorCode::Failed)
{
 if (!value.has_value()) {
  return Result<T>(Status::fail(code));
 }
 return Result<T>(*value);
}

template <typename T>
inline Result<T> OrElseResult(const std::optional<T>& value, ErrorCode code = ErrorCode::Failed)
{
 return ToResult(value, code);
}

template <typename T, typename Pred>
inline std::optional<T> Filter(const std::optional<T>& value, Pred&& pred)
{
 if (!value.has_value() || !pred(*value)) {
  return std::nullopt;
 }
 return value;
}

template <typename T>
inline T UnwrapOrDefault(const std::optional<T>& value)
{
 return value.has_value() ? *value : T{};
}

template <typename T>
inline bool HasValueOr(const std::optional<T>& value, bool fallback)
{
 return value.has_value() ? true : fallback;
}

template <typename T>
inline bool IsPresent(const std::optional<T>& value)
{
 return value.has_value();
}

template <typename T>
inline const T* ValuePtr(const std::optional<T>& value) noexcept
{
 return value.has_value() ? &*value : nullptr;
}

template <typename T>
inline T* ValuePtr(std::optional<T>& value) noexcept
{
 return value.has_value() ? &*value : nullptr;
}

template <typename T>
inline bool HasValuePtr(const std::optional<T>& value) noexcept
{
 return ValuePtr(value) != nullptr;
}

template <typename T>
inline bool IsEmpty(const std::optional<T>& value) noexcept
{
 return !value.has_value();
}

template <typename T>
inline const T* GetValuePtr(const std::optional<T>& value) noexcept
{
 return ValuePtr(value);
}

template <typename T>
inline T ValueOrDefault(const std::optional<T>& value)
{
 return UnwrapOrDefault(value);
}

template <typename T>
inline bool HasAnyValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline const T* Ptr(const std::optional<T>& value) noexcept
{
 return ValuePtr(value);
}

template <typename T>
inline bool HasAny(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline const T* ToPtr(const std::optional<T>& value) noexcept
{
 return ValuePtr(value);
}

template <typename T>
inline bool HasContent(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool ContainsValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPopulated(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasData(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool NotEmpty(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasElements(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasEntries(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Some(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasSome(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Any(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Exists(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Present(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool Available(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool ExistsValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasPresence(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasBeing(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsThere(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasExistence(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsExisting(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasBeingValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsAlive(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentBeing(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasValueState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool ExistsState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool StateExists(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsSet(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasPresence(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasDataValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsAvailable(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasAnyValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsThereValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsPresentData(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasPresentData(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasStoredValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsStored(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasContainedValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsHeld(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKept(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKept(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptValueState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKeptValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKeptState(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptPresence(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsKeptPresence(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool HasKeptStateValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

template <typename T>
inline bool IsStoredValue(const std::optional<T>& value) noexcept
{
 return HasValue(value);
}

}
