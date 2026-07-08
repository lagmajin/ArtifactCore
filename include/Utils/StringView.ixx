module;
#include <QByteArray>
#include <QString>
#include <QStringView>
#include <string_view>

export module Utils.String.View;

export namespace ArtifactCore {

using StringView = QStringView;

inline QString toQString(StringView view) {
  return view.toString();
}

inline std::string toStdString(StringView view) {
  const QByteArray utf8 = view.toString().toUtf8();
  return std::string(utf8.constData(), static_cast<size_t>(utf8.size()));
}

inline std::string_view toStdStringView(StringView view) {
  const QString text = view.toString();
  static thread_local std::string storage;
  const QByteArray utf8 = text.toUtf8();
  storage.assign(utf8.constData(), static_cast<size_t>(utf8.size()));
  return std::string_view(storage);
}

inline StringView toStringView(const QString& value) {
  return StringView(value);
}

inline QString toQString(std::string_view view) {
  return QString::fromUtf8(view.data(), static_cast<int>(view.size()));
}

inline bool startsWith(StringView value, StringView prefix) {
  return value.startsWith(prefix);
}

inline bool endsWith(StringView value, StringView suffix) {
  return value.endsWith(suffix);
}

inline bool isEmpty(StringView value) {
  return value.isEmpty();
}

inline bool contains(StringView value, StringView needle) {
  return value.contains(needle);
}

inline bool isNotEmpty(StringView value) {
  return !value.isEmpty();
}

inline bool isNullOrEmpty(StringView value) {
  return value.isNull() || value.isEmpty();
}

inline std::string_view toStdView(StringView view) {
  return toStdStringView(view);
}

inline bool hasPrefix(StringView value, StringView prefix) {
  return startsWith(value, prefix);
}

inline bool hasSuffix(StringView value, StringView suffix) {
  return endsWith(value, suffix);
}

inline bool containsText(StringView value, StringView needle) {
  return contains(value, needle);
}

inline bool isBlank(StringView value) {
  return value.trimmed().isEmpty();
}

inline bool notEmpty(StringView value) {
  return isNotEmpty(value);
}

inline bool notNullOrEmpty(StringView value) {
  return !isNullOrEmpty(value);
}

inline bool empty(StringView value) {
  return isEmpty(value);
}

inline bool hasText(StringView value) {
  return !isEmpty(value);
}

inline bool isNotBlank(StringView value) {
  return !isBlank(value);
}

inline bool hasContent(StringView value) {
  return hasText(value);
}

inline bool containsAnyText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool matchesPrefix(StringView value, StringView prefix) {
  return hasPrefix(value, prefix);
}

inline bool matchesSuffix(StringView value, StringView suffix) {
  return hasSuffix(value, suffix);
}

inline bool hasPrefixText(StringView value, StringView prefix) {
  return matchesPrefix(value, prefix);
}

inline bool hasSuffixText(StringView value, StringView suffix) {
  return matchesSuffix(value, suffix);
}

inline bool startsWithText(StringView value, StringView prefix) {
  return startsWith(value, prefix);
}

inline bool endsWithText(StringView value, StringView suffix) {
  return endsWith(value, suffix);
}

inline bool containsPrefix(StringView value, StringView prefix) {
  return hasPrefix(value, prefix);
}

inline bool startsWithAny(StringView value, StringView prefix) {
  return startsWith(value, prefix);
}

inline bool endsWithAny(StringView value, StringView suffix) {
  return endsWith(value, suffix);
}

inline bool containsAny(StringView value, StringView needle) {
  return contains(value, needle);
}

inline bool hasAnyText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasTextAny(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool containsTextAny(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasAny(StringView value, StringView needle) {
  return contains(value, needle);
}

inline bool containsValue(StringView value, StringView needle) {
  return contains(value, needle);
}

inline bool hasContains(StringView value, StringView needle) {
  return contains(value, needle);
}

inline bool containsTextValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasContainsValue(StringView value, StringView needle) {
  return contains(value, needle);
}

inline bool matchesText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasTextValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textContains(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textMatches(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textIncludes(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textHas(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textContainsValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textIncludesValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textHasValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textContainsNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool containsNeedleText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool needleContainsText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textNeedleContains(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool valueContainsText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textHasNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool needleHasText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textOwnsNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool ownsTextNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasTextNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool needleHasValueText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textValueHasNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool valueHasNeedleText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasNeedleValueText(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool valueHasTextNeedle(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool needleTextHasValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool hasNeedleTextValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool textNeedleHasValue(StringView value, StringView needle) {
  return containsText(value, needle);
}

inline bool needleTextValueHas(StringView value, StringView needle) {
  return containsText(value, needle);
}

}
