module;
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

export module Utils.Text.Number;

import Utils.Result;

export namespace ArtifactCore {

inline Result<std::int64_t> parseInt64(std::string_view value,
                                      std::string objectId = {})
{
  if (value.empty()) {
    return Result<std::int64_t>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "integer input is empty",
        .operation = "number.parseInt64",
        .objectId = std::move(objectId),
        .location = sourceLocation(__FILE__, __func__, __LINE__)});
  }
  std::int64_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    return Result<std::int64_t>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "invalid integer input",
        .operation = "number.parseInt64",
        .objectId = std::move(objectId),
        .location = sourceLocation(__FILE__, __func__, __LINE__)});
  }
  return Result<std::int64_t>::ok(parsed);
}

inline Result<std::uint64_t> parseUInt64(std::string_view value,
                                         std::string objectId = {})
{
  if (value.empty()) {
    return Result<std::uint64_t>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "unsigned integer input is empty",
        .operation = "number.parseUInt64",
        .objectId = std::move(objectId),
        .location = sourceLocation(__FILE__, __func__, __LINE__)});
  }
  std::uint64_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    return Result<std::uint64_t>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "invalid unsigned integer input",
        .operation = "number.parseUInt64",
        .objectId = std::move(objectId),
        .location = sourceLocation(__FILE__, __func__, __LINE__)});
  }
  return Result<std::uint64_t>::ok(parsed);
}

inline Result<bool> parseBool(std::string_view value,
                              std::string objectId = {})
{
  if (value == "true" || value == "1") return Result<bool>::ok(true);
  if (value == "false" || value == "0") return Result<bool>::ok(false);
  return Result<bool>::fail(ErrorContext{
      .code = ErrorCode::InvalidArgument,
      .message = "invalid boolean input",
      .operation = "number.parseBool",
      .objectId = std::move(objectId),
      .location = sourceLocation(__FILE__, __func__, __LINE__)});
}

} // namespace ArtifactCore
