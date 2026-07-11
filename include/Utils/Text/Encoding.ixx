module;
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

export module Utils.Text.Encoding;

import Utils.Result;

export namespace ArtifactCore {

enum class ByteOrderMark : std::uint8_t {
  None,
  Utf8,
  Utf16LittleEndian,
  Utf16BigEndian,
  Utf32LittleEndian,
  Utf32BigEndian
};

inline ByteOrderMark detectBom(std::string_view bytes) noexcept
{
  if (bytes.size() >= 4 &&
      static_cast<std::uint8_t>(bytes[0]) == 0xff &&
      static_cast<std::uint8_t>(bytes[1]) == 0xfe &&
      static_cast<std::uint8_t>(bytes[2]) == 0x00 &&
      static_cast<std::uint8_t>(bytes[3]) == 0x00) {
    return ByteOrderMark::Utf32LittleEndian;
  }
  if (bytes.size() >= 4 &&
      static_cast<std::uint8_t>(bytes[0]) == 0x00 &&
      static_cast<std::uint8_t>(bytes[1]) == 0x00 &&
      static_cast<std::uint8_t>(bytes[2]) == 0xfe &&
      static_cast<std::uint8_t>(bytes[3]) == 0xff) {
    return ByteOrderMark::Utf32BigEndian;
  }
  if (bytes.size() >= 3 &&
      static_cast<std::uint8_t>(bytes[0]) == 0xef &&
      static_cast<std::uint8_t>(bytes[1]) == 0xbb &&
      static_cast<std::uint8_t>(bytes[2]) == 0xbf) {
    return ByteOrderMark::Utf8;
  }
  if (bytes.size() >= 2 &&
      static_cast<std::uint8_t>(bytes[0]) == 0xff &&
      static_cast<std::uint8_t>(bytes[1]) == 0xfe) {
    return ByteOrderMark::Utf16LittleEndian;
  }
  if (bytes.size() >= 2 &&
      static_cast<std::uint8_t>(bytes[0]) == 0xfe &&
      static_cast<std::uint8_t>(bytes[1]) == 0xff) {
    return ByteOrderMark::Utf16BigEndian;
  }
  return ByteOrderMark::None;
}

inline std::size_t bomSize(ByteOrderMark bom) noexcept
{
  switch (bom) {
  case ByteOrderMark::Utf8: return 3;
  case ByteOrderMark::Utf16LittleEndian:
  case ByteOrderMark::Utf16BigEndian: return 2;
  case ByteOrderMark::Utf32LittleEndian:
  case ByteOrderMark::Utf32BigEndian: return 4;
  case ByteOrderMark::None: return 0;
  }
  return 0;
}

inline std::string_view stripBom(std::string_view bytes) noexcept
{
  const auto size = bomSize(detectBom(bytes));
  return size <= bytes.size() ? bytes.substr(size) : std::string_view{};
}

inline bool isValidUtf8(std::string_view value) noexcept
{
  const auto byteAt = [&value](std::size_t index) {
    return static_cast<std::uint8_t>(value[index]);
  };
  std::size_t index = 0;
  while (index < value.size()) {
    const std::uint8_t lead = byteAt(index);
    std::size_t length = 0;
    if (lead <= 0x7f) {
      length = 1;
    } else if (lead >= 0xc2 && lead <= 0xdf) {
      length = 2;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      length = 3;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      length = 4;
    } else {
      return false;
    }
    if (index + length > value.size()) return false;
    for (std::size_t offset = 1; offset < length; ++offset) {
      if ((byteAt(index + offset) & 0xc0) != 0x80) return false;
    }
    if (length == 3) {
      const std::uint8_t second = byteAt(index + 1);
      if ((lead == 0xe0 && second < 0xa0) ||
          (lead == 0xed && second > 0x9f)) return false;
    } else if (length == 4) {
      const std::uint8_t second = byteAt(index + 1);
      if ((lead == 0xf0 && second < 0x90) ||
          (lead == 0xf4 && second > 0x8f)) return false;
    }
    index += length;
  }
  return true;
}

inline std::optional<std::size_t> firstInvalidUtf8Offset(std::string_view value) noexcept
{
  const auto byteAt = [&value](std::size_t index) {
    return static_cast<std::uint8_t>(value[index]);
  };
  for (std::size_t index = 0; index < value.size();) {
    const std::uint8_t lead = byteAt(index);
    std::size_t length = 0;
    if (lead <= 0x7f) length = 1;
    else if (lead >= 0xc2 && lead <= 0xdf) length = 2;
    else if (lead >= 0xe0 && lead <= 0xef) length = 3;
    else if (lead >= 0xf0 && lead <= 0xf4) length = 4;
    else return index;
    if (index + length > value.size()) return index;
    for (std::size_t offset = 1; offset < length; ++offset) {
      if ((byteAt(index + offset) & 0xc0) != 0x80) return index + offset;
    }
    if ((length == 3 &&
         ((lead == 0xe0 && byteAt(index + 1) < 0xa0) ||
          (lead == 0xed && byteAt(index + 1) > 0x9f))) ||
        (length == 4 &&
         ((lead == 0xf0 && byteAt(index + 1) < 0x90) ||
          (lead == 0xf4 && byteAt(index + 1) > 0x8f)))) {
      return index;
    }
    index += length;
  }
  return std::nullopt;
}

inline Result<std::string> fromUtf8Checked(std::string_view value)
{
  if (const auto invalidOffset = firstInvalidUtf8Offset(value);
      invalidOffset.has_value()) {
    return Result<std::string>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "input contains invalid UTF-8 at byte " +
                   std::to_string(*invalidOffset),
        .operation = "encoding.fromUtf8Checked",
        .objectId = std::to_string(*invalidOffset),
        .location = ARTIFACT_CORE_SOURCE_LOCATION});
  }
  return Result<std::string>::ok(std::string(value));
}

inline Result<std::string> fromUtf8BomAware(std::string_view value)
{
  auto checked = fromUtf8Checked(stripBom(value));
  if (!checked) {
    auto context = checked.errorContext();
    context.operation = "encoding.fromUtf8BomAware";
    return Result<std::string>::fail(std::move(context));
  }
  return checked;
}

inline Result<std::u32string> toUtf32Checked(std::string_view value)
{
  if (const auto invalidOffset = firstInvalidUtf8Offset(value);
      invalidOffset.has_value()) {
    return Result<std::u32string>::fail(ErrorContext{
        .code = ErrorCode::InvalidArgument,
        .message = "input contains invalid UTF-8 at byte " +
                   std::to_string(*invalidOffset),
        .operation = "encoding.toUtf32Checked",
        .objectId = std::to_string(*invalidOffset),
        .location = ARTIFACT_CORE_SOURCE_LOCATION});
  }

  std::u32string result;
  result.reserve(value.size());
  const auto byteAt = [&value](std::size_t index) {
    return static_cast<std::uint8_t>(value[index]);
  };
  for (std::size_t index = 0; index < value.size();) {
    const std::uint8_t lead = byteAt(index);
    if (lead <= 0x7f) {
      result.push_back(static_cast<char32_t>(lead));
      index += 1;
    } else if (lead <= 0xdf) {
      result.push_back(static_cast<char32_t>(lead & 0x1f) << 6 |
                      static_cast<char32_t>(byteAt(index + 1) & 0x3f));
      index += 2;
    } else if (lead <= 0xef) {
      result.push_back(static_cast<char32_t>(lead & 0x0f) << 12 |
                      static_cast<char32_t>(byteAt(index + 1) & 0x3f) << 6 |
                      static_cast<char32_t>(byteAt(index + 2) & 0x3f));
      index += 3;
    } else {
      result.push_back(static_cast<char32_t>(lead & 0x07) << 18 |
                      static_cast<char32_t>(byteAt(index + 1) & 0x3f) << 12 |
                      static_cast<char32_t>(byteAt(index + 2) & 0x3f) << 6 |
                      static_cast<char32_t>(byteAt(index + 3) & 0x3f));
      index += 4;
    }
  }
  return Result<std::u32string>::ok(std::move(result));
}

inline Result<std::size_t> utf8CodepointCount(std::string_view value)
{
  auto converted = toUtf32Checked(value);
  if (!converted) {
    return Result<std::size_t>::fail(converted.errorContext());
  }
  return Result<std::size_t>::ok(converted.value().size());
}

} // namespace ArtifactCore
