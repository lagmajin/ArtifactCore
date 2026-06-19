module;
#include <optional>
#include <type_traits>
#include <utility>

export module Utils.Result;

export namespace ArtifactCore {

enum class ErrorCode {
  None = 0,
  Unknown,
  InvalidArgument,
  NotFound,
  AlreadyExists,
  Busy,
  Cancelled,
  Failed
};

struct Status {
  bool success{false};
  ErrorCode error{ErrorCode::None};

  static constexpr Status ok() noexcept { return {true, ErrorCode::None}; }
  static constexpr Status fail(ErrorCode code = ErrorCode::Failed) noexcept { return {false, code}; }
  explicit constexpr operator bool() const noexcept { return success; }
};

template <typename T>
class Result {
 public:
  Result() = default;
  Result(const T& value) : value_(value), status_(Status::ok()) {}
  Result(T&& value) : value_(std::move(value)), status_(Status::ok()) {}
  Result(Status status) : status_(status) {}

  static Result ok(T value) { return Result(std::move(value)); }
  static Result fail(ErrorCode code = ErrorCode::Failed) { return Result(Status::fail(code)); }

  bool success() const noexcept { return status_.success; }
  bool isOk() const noexcept { return success(); }
  bool ok() const noexcept { return success(); }
  bool succeeded() const noexcept { return success(); }
  bool passed() const noexcept { return success(); }
  bool hasValue() const noexcept { return value_.has_value(); }
  bool hasError() const noexcept { return !status_.success; }
  ErrorCode error() const noexcept { return status_.error; }
  explicit operator bool() const noexcept { return success(); }
  const Status& status() const noexcept { return status_; }

  T& value() & { return *value_; }
  const T& value() const& { return *value_; }
  T&& value() && { return std::move(*value_); }
  T* valuePtr() noexcept { return value_ ? &*value_ : nullptr; }
  const T* valuePtr() const noexcept { return value_ ? &*value_ : nullptr; }
  T valueOr(T fallback) const {
    return value_.has_value() ? *value_ : std::move(fallback);
  }
  T unwrapOr(T fallback) const {
    return valueOr(std::move(fallback));
  }
  template <typename F>
  T valueOrElse(F&& fallbackFactory) const {
    return value_.has_value() ? *value_ : static_cast<T>(fallbackFactory());
  }

  const std::optional<T>& maybe() const noexcept { return value_; }
  std::optional<T> toOptional() const { return value_; }
  const T* getIfValue() const noexcept { return valuePtr(); }

  template <typename F>
  auto transform(F&& fn) const -> Result<std::decay_t<decltype(fn(std::declval<const T&>()))>>
  {
    using U = std::decay_t<decltype(fn(std::declval<const T&>()))>;
    if (!value_.has_value()) {
      return Result<U>(status_);
    }
    return Result<U>(fn(*value_));
  }

  template <typename F>
  auto map(F&& fn) const -> Result<std::decay_t<decltype(fn(std::declval<const T&>()))>>
  {
    return transform(std::forward<F>(fn));
  }

  template <typename F>
  auto andThen(F&& fn) const -> decltype(fn(std::declval<const T&>()))
  {
    using R = decltype(fn(std::declval<const T&>()));
    if (!value_.has_value()) {
      return R(status_);
    }
    return fn(*value_);
  }

  template <typename F>
  auto orElse(F&& fn) const -> Result<T>
  {
    if (value_.has_value()) {
      return *this;
    }
    return fn(status_);
  }

  bool isFailed() const noexcept { return !success(); }
  bool failed() const noexcept { return isFailed(); }
  bool empty() const noexcept { return !hasValue(); }
  bool hasAnyValue() const noexcept { return hasValue(); }
  bool notEmpty() const noexcept { return hasValue(); }
  bool isPopulated() const noexcept { return hasValue(); }
  bool populated() const noexcept { return hasValue(); }
  const Status& getStatus() const noexcept { return status_; }
  bool statusOk() const noexcept { return static_cast<bool>(status_); }
  ErrorCode errorOr(ErrorCode fallback) const noexcept { return status_.success ? fallback : status_.error; }
  const std::optional<T>& asOptional() const noexcept { return maybe(); }
  T valueOrDefault() const { return value_.has_value() ? *value_ : T{}; }
  T valueOrElse(const T& fallback) const { return value_.has_value() ? *value_ : fallback; }
  T* tryValue() noexcept { return valuePtr(); }
  const T* tryValue() const noexcept { return valuePtr(); }
  bool hasValuePtr() const noexcept { return valuePtr() != nullptr; }
  template <typename F>
  auto ifValue(F&& fn) const -> const Result&
  {
    if (value_.has_value()) {
      fn(*value_);
    }
    return *this;
  }

private:
  std::optional<T> value_;
  Status status_{Status::fail()};
};

template <typename T>
inline Result<T> MakeResult(T value)
{
 return Result<T>(std::move(value));
}

template <typename T>
inline Result<T> MakeFailedResult(ErrorCode code = ErrorCode::Failed)
{
 return Result<T>(Status::fail(code));
}

template <typename T>
inline Result<T> FromOptional(const std::optional<T>& value, ErrorCode code = ErrorCode::Failed)
{
 if (!value.has_value()) {
  return Result<T>(Status::fail(code));
 }
 return Result<T>(*value);
}

using VoidResult = Status;

}
