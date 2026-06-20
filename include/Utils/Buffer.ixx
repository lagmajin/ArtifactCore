module;
#include <cstddef>
#include <cstdint>
#include <QString>
#include <QStringView>
#include <QByteArray>
#include <QByteArrayView>
#include <initializer_list>
#include <utility>
#include <span>
#include <string_view>

export module Utils.Buffer;

export namespace ArtifactCore {

using BufferView = QByteArrayView;

inline QByteArray toQByteArray(BufferView view) {
  return QByteArray(view.data(), view.size());
}

inline QByteArray toQByteArray(const QByteArray& value) {
  return value;
}

inline QByteArray toQByteArray(BufferView lhs, BufferView rhs) {
  QByteArray out;
  out.reserve(static_cast<int>(lhs.size() + rhs.size()));
  out.append(lhs.data(), lhs.size());
  out.append(rhs.data(), rhs.size());
  return out;
}

inline QByteArray toQByteArray(std::initializer_list<BufferView> parts) {
  QByteArray out;
  std::size_t total = 0;
  for (BufferView part : parts) {
    total += part.size();
  }
  out.reserve(static_cast<int>(total));
  for (BufferView part : parts) {
    out.append(part.data(), part.size());
  }
  return out;
}

inline std::string toStdString(BufferView view) {
  return std::string(view.data(), view.size());
}

inline QString toQString(BufferView view) {
  return QString::fromUtf8(view.data(), static_cast<int>(view.size()));
}

inline BufferView toBufferView(const QString& value) {
  const QByteArray utf8 = value.toUtf8();
  return BufferView(utf8.constData(), utf8.size());
}

inline BufferView toBufferView(QStringView value) {
  return BufferView(reinterpret_cast<const char*>(value.data()),
                    static_cast<int>(value.size() * sizeof(QChar)));
}

inline BufferView toBufferView(const QByteArray& value) {
  return BufferView(value);
}

inline BufferView toBufferView(const std::uint8_t* data, std::size_t size) {
  return BufferView(reinterpret_cast<const char*>(data), static_cast<int>(size));
}

inline BufferView toBufferView(std::span<const std::uint8_t> data) {
  return toBufferView(data.data(), data.size());
}

inline BufferView toBufferView(std::string_view view) {
  return BufferView(view.data(), static_cast<int>(view.size()));
}

inline QByteArray appendToQByteArray(QByteArray base, BufferView extra) {
  base.append(extra.data(), extra.size());
  return base;
}

inline bool startsWith(BufferView value, BufferView prefix)
{
  return value.size() >= prefix.size()
      && QByteArrayView(value.data(), prefix.size()) == prefix;
}

inline bool endsWith(BufferView value, BufferView suffix)
{
  return value.size() >= suffix.size()
      && QByteArrayView(value.data() + (value.size() - suffix.size()), suffix.size()) == suffix;
}

inline bool contains(BufferView value, BufferView needle)
{
  return needle.isEmpty() || QByteArrayView(value).contains(needle);
}

inline BufferView trim(BufferView value)
{
  auto begin = value.begin();
  auto end = value.end();
  while (begin != end && (*begin == ' ' || *begin == '\t' || *begin == '\n' || *begin == '\r')) {
    ++begin;
  }
  while (end != begin) {
    const auto prev = end - 1;
    if (*prev != ' ' && *prev != '\t' && *prev != '\n' && *prev != '\r') {
      break;
    }
    --end;
  }
  return BufferView(begin, static_cast<int>(end - begin));
}

inline BufferView ltrim(BufferView value)
{
  auto begin = value.begin();
  const auto end = value.end();
  while (begin != end && (*begin == ' ' || *begin == '\t' || *begin == '\n' || *begin == '\r')) {
    ++begin;
  }
  return BufferView(begin, static_cast<int>(end - begin));
}

inline BufferView rtrim(BufferView value)
{
  const auto begin = value.begin();
  auto end = value.end();
  while (end != begin) {
    const auto prev = end - 1;
    if (*prev != ' ' && *prev != '\t' && *prev != '\n' && *prev != '\r') {
      break;
    }
    --end;
  }
  return BufferView(begin, static_cast<int>(end - begin));
}

inline std::pair<BufferView, BufferView> split(BufferView value, std::size_t leftSize)
{
  const auto clamped = static_cast<int>(leftSize > value.size() ? value.size() : leftSize);
  return {
    BufferView(value.data(), clamped),
    BufferView(value.data() + clamped, static_cast<int>(value.size()) - clamped)
  };
}

inline std::pair<BufferView, BufferView> splitAt(BufferView value, int index)
{
  const auto clamped = index < 0 ? 0 : (index > value.size() ? value.size() : index);
  return {
    BufferView(value.data(), clamped),
    BufferView(value.data() + clamped, static_cast<int>(value.size()) - clamped)
  };
}

inline std::pair<BufferView, BufferView> splitFirst(BufferView value, char delimiter)
{
  const auto pos = value.indexOf(delimiter);
  if (pos < 0) {
    return {value, BufferView()};
  }
  return splitAt(value, pos);
}

inline BufferView beforeFirst(BufferView value, char delimiter)
{
  return splitFirst(value, delimiter).first;
}

inline BufferView afterFirst(BufferView value, char delimiter)
{
  const auto parts = splitFirst(value, delimiter);
  if (parts.second.isEmpty()) {
    return BufferView();
  }
  return BufferView(parts.second.data() + 1, parts.second.size() - 1);
}

inline std::pair<BufferView, BufferView> splitLast(BufferView value, char delimiter)
{
  const auto pos = value.lastIndexOf(delimiter);
  if (pos < 0) {
    return {BufferView(), value};
  }
  return splitAt(value, pos);
}

inline BufferView beforeLast(BufferView value, char delimiter)
{
  return splitLast(value, delimiter).first;
}

inline BufferView afterLast(BufferView value, char delimiter)
{
  const auto parts = splitLast(value, delimiter);
  if (parts.second.isEmpty()) {
    return BufferView();
  }
  return BufferView(parts.second.data() + 1, parts.second.size() - 1);
}

inline BufferView afterPrefix(BufferView value, BufferView prefix)
{
  return startsWith(value, prefix) ? BufferView(value.data() + prefix.size(), value.size() - prefix.size()) : BufferView();
}

inline BufferView beforeSuffix(BufferView value, BufferView suffix)
{
  return endsWith(value, suffix) ? BufferView(value.data(), value.size() - suffix.size()) : BufferView();
}

inline BufferView afterSuffix(BufferView value, BufferView suffix)
{
  return endsWith(value, suffix) ? BufferView(value.data() + value.size() - suffix.size(), suffix.size()) : BufferView();
}

inline BufferView afterSeparator(BufferView value, char delimiter)
{
  return afterFirst(value, delimiter);
}

inline BufferView beforeSeparator(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView afterDelimiter(BufferView value, char delimiter)
{
  return afterFirst(value, delimiter);
}

inline BufferView beforeDelimiter(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView prefixBefore(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView suffixAfter(BufferView value, char delimiter)
{
  return afterLast(value, delimiter);
}

inline BufferView tailAfter(BufferView value, char delimiter)
{
  return afterLast(value, delimiter);
}

inline BufferView headBefore(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView prefixOf(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView suffixOf(BufferView value, char delimiter)
{
  return afterLast(value, delimiter);
}

inline BufferView trailingPart(BufferView value, char delimiter)
{
  return afterLast(value, delimiter);
}

inline BufferView leadingPart(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView takeBefore(BufferView value, char delimiter)
{
  return beforeFirst(value, delimiter);
}

inline BufferView takeAfter(BufferView value, char delimiter)
{
  return afterLast(value, delimiter);
}

inline bool hasPrefix(BufferView value, BufferView prefix)
{
  return startsWith(value, prefix);
}

inline bool hasSuffix(BufferView value, BufferView suffix)
{
  return endsWith(value, suffix);
}

inline BufferView trimLeft(BufferView value)
{
  return ltrim(value);
}

inline BufferView trimRight(BufferView value)
{
  return rtrim(value);
}

inline BufferView trimStart(BufferView value)
{
  return ltrim(value);
}

inline BufferView trimEnd(BufferView value)
{
  return rtrim(value);
}

inline QByteArray repeat(BufferView value, int count)
{
  QByteArray out;
  if (count <= 0 || value.isEmpty()) {
    return out;
  }
  out.reserve(static_cast<int>(value.size()) * count);
  for (int i = 0; i < count; ++i) {
    out.append(value.data(), value.size());
  }
  return out;
}

}
