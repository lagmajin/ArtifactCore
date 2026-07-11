module;
#include <algorithm>
#include <cstddef>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

export module Utils.Text.Path;

import Utils.Result;

export namespace ArtifactCore {

inline bool isAbsolutePath(std::string_view value) noexcept
{
  if (value.empty()) return false;
  if (value.front() == '/' || value.front() == '\\') return true;
  return value.size() >= 3 &&
         std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
         value[1] == ':' && (value[2] == '/' || value[2] == '\\');
}

inline Result<std::string> normalizePathSeparators(std::string_view value,
                                                    std::string objectId = {})
{
  if (value.find('\0') != std::string_view::npos) {
    return Result<std::string>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "path contains NUL",
        .operation = "path.normalizeSeparators",
        .objectId = std::move(objectId),
        .location = ARTIFACT_CORE_SOURCE_LOCATION});
  }
  std::string result;
  result.reserve(value.size());
  bool previousSeparator = false;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char character = value[index];
    const bool separator = character == '/' || character == '\\';
    if (separator) {
      const bool preserveUncPrefix = index == 1 && previousSeparator &&
                                     (value[0] == '/' || value[0] == '\\');
      if (previousSeparator && !preserveUncPrefix) continue;
      result.push_back('/');
    } else {
      result.push_back(character);
    }
    previousSeparator = separator;
  }
  return Result<std::string>::ok(std::move(result));
}

inline bool hasParentTraversal(std::string_view value) noexcept
{
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t slash = value.find('/', start);
    const std::size_t backslash = value.find('\\', start);
    const std::size_t end = slash == std::string_view::npos
        ? backslash
        : (backslash == std::string_view::npos ? slash : std::min(slash, backslash));
    const std::string_view segment = value.substr(
        start, end == std::string_view::npos ? value.size() - start : end - start);
    if (segment == "..") return true;
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return false;
}

inline bool isSafeRelativePath(std::string_view value) noexcept
{
  return !value.empty() && !isAbsolutePath(value) && !hasParentTraversal(value);
}

} // namespace ArtifactCore
