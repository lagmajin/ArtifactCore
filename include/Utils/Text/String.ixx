module;
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module Utils.Text.String;

export namespace ArtifactCore {

inline bool isAsciiWhitespace(char value) noexcept
{
  return value == ' ' || value == '\t' || value == '\n' ||
         value == '\r' || value == '\f' || value == '\v';
}

inline std::string_view trimView(std::string_view value) noexcept
{
  std::size_t first = 0;
  while (first < value.size() && isAsciiWhitespace(value[first])) ++first;
  std::size_t last = value.size();
  while (last > first && isAsciiWhitespace(value[last - 1])) --last;
  return value.substr(first, last - first);
}

inline bool startsWith(std::string_view value, std::string_view prefix) noexcept
{
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

inline bool endsWith(std::string_view value, std::string_view suffix) noexcept
{
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::vector<std::string_view> splitView(std::string_view value, char separator)
{
  std::vector<std::string_view> result;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t end = value.find(separator, start);
    result.push_back(value.substr(
        start, end == std::string_view::npos ? value.size() - start : end - start));
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return result;
}

inline std::string join(std::span<const std::string_view> parts, char separator)
{
  std::size_t size = parts.empty() ? 0 : parts.size() - 1;
  for (const auto part : parts) size += part.size();
  std::string result;
  result.reserve(size);
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index != 0) result.push_back(separator);
    result.append(parts[index]);
  }
  return result;
}

} // namespace ArtifactCore
