module;
#include <optional>
#include <cstdint>
#include <string>
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

inline constexpr const char* errorCodeName(ErrorCode code) noexcept
{
  switch (code) {
  case ErrorCode::None: return "none";
  case ErrorCode::Unknown: return "unknown";
  case ErrorCode::InvalidArgument: return "invalid_argument";
  case ErrorCode::NotFound: return "not_found";
  case ErrorCode::AlreadyExists: return "already_exists";
  case ErrorCode::Busy: return "busy";
  case ErrorCode::Cancelled: return "cancelled";
  case ErrorCode::Failed: return "failed";
  }
  return "unknown";
}

struct SourceLocation {
  const char* file = "";
  const char* function = "";
  int line = 0;

  bool hasValue() const noexcept
  {
    return file != nullptr && file[0] != '\0' && line > 0;
  }
};

struct ErrorContext {
  ErrorCode code = ErrorCode::None;
  std::string message;
  std::string operation;
  std::string objectId;
  SourceLocation location;
  std::uint64_t traceId = 0;
};

inline constexpr SourceLocation sourceLocation(
    const char* file, const char* function, int line) noexcept
{
  return SourceLocation{file, function, line};
}

#define ARTIFACT_CORE_SOURCE_LOCATION \
  ::ArtifactCore::sourceLocation(__FILE__, __func__, __LINE__)

struct Status {
  bool success{false};
  ErrorCode error{ErrorCode::None};
  ErrorContext context{};

  static Status ok() noexcept { return {true, ErrorCode::None, {}}; }
  static Status fail(ErrorCode code = ErrorCode::Failed) noexcept {
    return {false, code, ErrorContext{.code = code}};
  }
  static Status fail(ErrorCode code,
                     std::string message,
                     std::string operation = {},
                     std::string objectId = {},
                     SourceLocation location = {}) {
    return fail(ErrorContext{
        .code = code,
        .message = std::move(message),
        .operation = std::move(operation),
        .objectId = std::move(objectId),
        .location = location});
  }
  static Status fail(ErrorContext context) noexcept {
    context.code = context.code == ErrorCode::None ? ErrorCode::Failed : context.code;
    return {false, context.code, std::move(context)};
  }
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
  static Result fail(ErrorCode code,
                     std::string message,
                     std::string operation = {},
                     std::string objectId = {},
                     SourceLocation location = {}) {
    return Result(Status::fail(code,
                                std::move(message),
                                std::move(operation),
                                std::move(objectId),
                                location));
  }
  static Result fail(ErrorContext context) {
    return Result(Status::fail(std::move(context)));
  }

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
  const ErrorContext& errorContext() const noexcept { return status_.context; }

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
inline Result<T> MakeFailedResult(ErrorContext context)
{
 return Result<T>(Status::fail(std::move(context)));
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
