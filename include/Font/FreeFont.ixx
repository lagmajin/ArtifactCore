module;
#include <utility>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QDebug>
#include <QDateTime>

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include <optional>
#include <unordered_map>

export module Font.FreeFont;

import Text.Style;
import Font.Descriptor;
import Core.Diagnostics.FallbackPolicy;
import Container.NamedVector;

export namespace ArtifactCore
{

class FontManager
{
public:
 static bool containsCjkCharacters(const QString& text)
 {
  for (const QChar ch : text) {
   const auto code = ch.unicode();
   if ((code >= 0x3040 && code <= 0x30FF) || // Hiragana / Katakana
       (code >= 0x3000 && code <= 0x303F) || // CJK punctuation
       (code >= 0x3400 && code <= 0x9FFF) || // CJK Unified Ideographs
       (code >= 0xF900 && code <= 0xFAFF) || // CJK Compatibility Ideographs
       (code >= 0xAC00 && code <= 0xD7AF) || // Hangul syllables
       (code >= 0x1100 && code <= 0x11FF) || // Hangul Jamo
       (code >= 0xFF00 && code <= 0xFFEF)) {  // Full-width forms / punctuation
    return true;
   }
  }
  return false;
 }

 static QStringList availableFamilies()
 {
  return QFontDatabase::families();
 }

 static std::vector<FontDescriptor> availableFonts()
 {
   NamedVector<FontDescriptor> fonts{
       makeNamedVector<FontDescriptor>(ContainerName{"FontManagerAvailableFonts"})};
   const QStringList families = QFontDatabase::families();
   for (const QString& family : families) {
    const QStringList styles = QFontDatabase::styles(family);
    if (styles.isEmpty()) {
     FontDescriptor descriptor;
     descriptor.family = family;
     descriptor.style = QStringLiteral("Regular");
     descriptor.isFixedPitch = QFontDatabase::isFixedPitch(family, QString());
     descriptor.weight = QFontDatabase::weight(family, QString());
     descriptor.italic = QFontDatabase::italic(family, QString());
     fonts.append(std::move(descriptor));
     continue;
    }
    for (const QString& style : styles) {
     FontDescriptor descriptor;
     descriptor.family = family;
     descriptor.style = style;
     descriptor.isFixedPitch = QFontDatabase::isFixedPitch(family, style);
     descriptor.weight = QFontDatabase::weight(family, style);
     descriptor.italic = QFontDatabase::italic(family, style);
     fonts.append(std::move(descriptor));
    }
   }
   std::sort(fonts.begin(), fonts.end(), [](const FontDescriptor& left,
                                             const FontDescriptor& right) {
    const int familyOrder = QString::compare(left.family, right.family,
                                              Qt::CaseInsensitive);
    return familyOrder != 0 ? familyOrder < 0
                            : QString::compare(left.style, right.style,
                                               Qt::CaseInsensitive) < 0;
   });
   return fonts.toStdVector();
 }

 static bool isFamilyAvailable(const QString& family)
 {
  if (family.trimmed().isEmpty()) {
   return false;
  }
  return QFontDatabase::families().contains(family, Qt::CaseInsensitive);
 }

 static QString defaultSansSerifFamily()
 {
  const QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  return font.family();
 }

 static QString defaultMonospaceFamily()
 {
  const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  return font.family();
 }

 static QStringList japaneseFallbackCandidates()
 {
  return {
      QStringLiteral("Yu Gothic UI"),
      QStringLiteral("Yu Gothic"),
      QStringLiteral("Meiryo UI"),
      QStringLiteral("Meiryo"),
      QStringLiteral("MS Gothic"),
      QStringLiteral("Noto Sans CJK JP"),
      QStringLiteral("Noto Sans JP"),
      QStringLiteral("Source Han Sans JP"),
      QStringLiteral("Segoe UI"),
  };
 }

 static QStringList emojiFallbackCandidates()
 {
  return {
      QStringLiteral("Segoe UI Emoji"),
      QStringLiteral("Noto Color Emoji"),
      QStringLiteral("Apple Color Emoji"),
      QStringLiteral("Segoe UI Symbol"),
  };
 }

 static bool containsEmojiCharacters(const QString& text)
 {
  const auto codePoints = text.toUcs4();
  for (const char32_t code : codePoints) {
   // Keep ordinary BMP symbols (for example ★) on the normal monochrome
   // font path.  The color-font fallback is reserved for supplementary-plane
   // emoji, where a dedicated emoji family is actually needed.
   if (code >= 0x1F000 && code <= 0x1FAFF) {
    return true;
   }
  }
  return false;
 }

 static QString firstAvailableFamily(const QStringList& candidates)
 {
  const QStringList families = availableFamilies();
  for (const QString& candidate : candidates) {
   if (candidate.isEmpty()) {
    continue;
   }
   if (families.contains(candidate, Qt::CaseInsensitive)) {
    return candidate;
   }
  }
  return {};
 }

 static QString scriptTagForCodePoint(UChar32 codePoint)
 {
  UErrorCode status = U_ZERO_ERROR;
  const UScriptCode script = uscript_getScript(codePoint, &status);
  if (U_FAILURE(status)) return {};
  const char* shortName = uscript_getShortName(script);
  if (!shortName) return {};
  return QString::fromLatin1(shortName);
 }

 static QStringList scriptFallbackCandidates(UChar32 codePoint)
 {
  const QString scriptTag = scriptTagForCodePoint(codePoint);
  if (scriptTag.isEmpty()) return {};
  if (scriptTag == QLatin1String("Arab")) {
   return {QStringLiteral("Noto Sans Arabic"), QStringLiteral("Noto Naskh Arabic"),
           QStringLiteral("Arabic Typesetting"), QStringLiteral("Traditional Arabic"),
           QStringLiteral("Segoe UI")};
  }
  if (scriptTag == QLatin1String("Hebr")) {
   return {QStringLiteral("Noto Sans Hebrew"), QStringLiteral("Arial"),
           QStringLiteral("Segoe UI")};
  }
  if (scriptTag == QLatin1String("Thai")) {
   return {QStringLiteral("Noto Sans Thai"), QStringLiteral("Leelawadee UI"),
           QStringLiteral("Tahoma")};
  }
  if (scriptTag == QLatin1String("Deva")) {
   return {QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Mangal")};
  }
  if (scriptTag == QLatin1String("Beng")) {
   return {QStringLiteral("Noto Sans Bengali"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Vrinda")};
  }
  if (scriptTag == QLatin1String("Guru")) {
   return {QStringLiteral("Noto Sans Gurmukhi"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Raavi")};
  }
  if (scriptTag == QLatin1String("Gujr")) {
   return {QStringLiteral("Noto Sans Gujarati"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Shruti")};
  }
  if (scriptTag == QLatin1String("Taml")) {
   return {QStringLiteral("Noto Sans Tamil"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Latha")};
  }
  if (scriptTag == QLatin1String("Telu")) {
   return {QStringLiteral("Noto Sans Telugu"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Gautami")};
  }
  if (scriptTag == QLatin1String("Knda")) {
   return {QStringLiteral("Noto Sans Kannada"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Tunga")};
  }
  if (scriptTag == QLatin1String("Mlym")) {
   return {QStringLiteral("Noto Sans Malayalam"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Kartika")};
  }
  if (scriptTag == QLatin1String("Sinh")) {
   return {QStringLiteral("Noto Sans Sinhala"), QStringLiteral("Nirmala UI"),
           QStringLiteral("Iskoola Pota")};
  }
  if (scriptTag == QLatin1String("Khmr")) {
   return {QStringLiteral("Noto Sans Khmer")};
  }
  if (scriptTag == QLatin1String("Mymr")) {
   return {QStringLiteral("Noto Sans Myanmar"), QStringLiteral("Myanmar Text")};
  }
  if (scriptTag == QLatin1String("Laoo")) {
   return {QStringLiteral("Noto Sans Lao"), QStringLiteral("Lao UI")};
  }
  if (scriptTag == QLatin1String("Armn")) {
   return {QStringLiteral("Noto Sans Armenian"), QStringLiteral("Sylfaen")};
  }
  if (scriptTag == QLatin1String("Geor")) {
   return {QStringLiteral("Noto Sans Georgian"), QStringLiteral("Sylfaen")};
  }
  if (scriptTag == QLatin1String("Ethi")) {
   return {QStringLiteral("Noto Sans Ethiopic"), QStringLiteral("Nyala")};
  }
  return {};
 }

 static int scriptCodeForCodePoint(UChar32 codePoint)
 {
  UErrorCode status = U_ZERO_ERROR;
  const int script = static_cast<int>(uscript_getScript(codePoint, &status));
  return U_FAILURE(status) ? -1 : script;
 }

 static bool shouldRecordScriptFallback(UChar32 codePoint)
 {
  constexpr int kScriptCacheSize = 256;
  const int script = scriptCodeForCodePoint(codePoint);
  if (script < 0 || script >= kScriptCacheSize) return true;
  static std::array<std::atomic_bool, kScriptCacheSize> recorded{};
  return !recorded[static_cast<size_t>(script)].exchange(
      true, std::memory_order_relaxed);
 }

 static bool rawFontSupportsText(const QRawFont& rawFont, const QString& text)
 {
  if (!rawFont.isValid()) return false;
  for (const char32_t code : text.toUcs4()) {
   if (u_isUWhiteSpace(static_cast<UChar32>(code)) ||
       u_hasBinaryProperty(static_cast<UChar32>(code), UCHAR_DEFAULT_IGNORABLE_CODE_POINT)) {
    continue;
   }
   const QString character = QString::fromUcs4(&code, 1);
   const auto glyphIndexes = rawFont.glyphIndexesForString(character);
   if (glyphIndexes.isEmpty() || glyphIndexes.front() == 0) return false;
  }
  return true;
 }

 static QString resolvedFamily(const QString& preferredFamily)
 {
  const QString preferred = preferredFamily.trimmed();
  if (!preferred.isEmpty() && isFamilyAvailable(preferred)) {
   return preferred;
  }

  auto* tracker = FallbackTracker::instance();
  const auto policy = tracker->policy(FallbackCategory::Font);

  if (!policy.enabled) {
   if (!preferred.isEmpty()) {
    qWarning() << "[FontManager] font missing and fallback disabled"
               << "requested=" << preferred;
   }
   return preferred;
  }

  const QString general = defaultSansSerifFamily();
  if (!general.isEmpty()) {
   tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                    FallbackAction::Fallback, preferred, general,
                    policy.warningMessage, policy.logWarning});
   return general;
  }

  const QString japaneseFallback = firstAvailableFamily(japaneseFallbackCandidates());
  if (!japaneseFallback.isEmpty()) {
   tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                    FallbackAction::Fallback, preferred, japaneseFallback,
                    policy.warningMessage, policy.logWarning});
   return japaneseFallback;
  }

  const QStringList families = availableFamilies();
  if (!families.isEmpty()) {
   tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                    FallbackAction::Fallback, preferred, families.front(),
                    policy.warningMessage, policy.logWarning});
   return families.front();
  }

  tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                   FallbackAction::Fallback, preferred, "Arial",
                   policy.warningMessage, policy.logWarning});
  return QStringLiteral("Arial");
 }

 static QString resolvedFamilyForText(const QString& preferredFamily, const QString& sampleText)
 {
  struct CacheEntry {
   QString preferredFamily;
   QString sampleText;
   QString resolvedFamily;
   std::uint64_t fontRevision = 0;
  };
  constexpr size_t kCacheSize = 8;
  thread_local std::array<CacheEntry, kCacheSize> cache{};
  thread_local size_t nextCacheSlot = 0;
  const std::uint64_t fontRevision = fontDatabaseRevision();
  for (const auto& entry : cache) {
   if (entry.fontRevision == fontRevision &&
       entry.preferredFamily == preferredFamily && entry.sampleText == sampleText) {
    return entry.resolvedFamily;
   }
  }
  QString resolved = resolveFamilyForTextUncached(preferredFamily, sampleText);
  auto& entry = cache[nextCacheSlot];
  entry.preferredFamily = preferredFamily;
  entry.sampleText = sampleText;
  entry.resolvedFamily = resolved;
  entry.fontRevision = fontRevision;
  nextCacheSlot = (nextCacheSlot + 1) % kCacheSize;
  return resolved;
 }

 static QString resolveFamilyForTextUncached(const QString& preferredFamily,
                                              const QString& sampleText)
 {
  const QString preferred = preferredFamily.trimmed();
  UChar32 firstMissingCodePoint = U_SENTINEL;
  UChar32 firstMissingAnyCodePoint = U_SENTINEL;
  const bool preferredAvailable = !preferred.isEmpty() && isFamilyAvailable(preferred);
   if (preferredAvailable) {
   QFont preferredFont(preferred);
   const QRawFont rawFont = QRawFont::fromFont(preferredFont, QFontDatabase::Any);
   bool needsFallback = false;
   for (const char32_t code : sampleText.toUcs4()) {
    if (u_isUWhiteSpace(static_cast<UChar32>(code)) ||
        u_hasBinaryProperty(static_cast<UChar32>(code), UCHAR_DEFAULT_IGNORABLE_CODE_POINT)) {
     continue;
    }
    const QString character = QString::fromUcs4(&code, 1);
    const auto glyphIndexes = rawFont.glyphIndexesForString(character);
    const bool missingInPreferred = glyphIndexes.isEmpty() || glyphIndexes.front() == 0;
    if (!rawFont.isValid() || missingInPreferred) {
     needsFallback = true;
     const UChar32 codePoint = static_cast<UChar32>(code);
     if (firstMissingAnyCodePoint == U_SENTINEL) {
      firstMissingAnyCodePoint = codePoint;
     }
     if (firstMissingCodePoint == U_SENTINEL &&
         !scriptFallbackCandidates(codePoint).isEmpty()) {
      firstMissingCodePoint = codePoint;
      break;
     }
    }
   }
   if (!needsFallback) {
    return preferred;
   }
  }

  if (!preferredAvailable) {
   for (const char32_t code : sampleText.toUcs4()) {
    const UChar32 codePoint = static_cast<UChar32>(code);
    if (u_isUWhiteSpace(codePoint) ||
        u_hasBinaryProperty(codePoint, UCHAR_DEFAULT_IGNORABLE_CODE_POINT)) {
     continue;
    }
    if (!scriptFallbackCandidates(codePoint).isEmpty()) {
     firstMissingCodePoint = codePoint;
     break;
    }
   }
  }

  const uint firstMissingCodePointValue =
      static_cast<uint>(firstMissingAnyCodePoint);
  const QString firstMissingCharacter =
      firstMissingAnyCodePoint != U_SENTINEL
          ? QString::fromUcs4(&firstMissingCodePointValue, 1)
          : QString{};
  const bool firstMissingIsEmoji = firstMissingAnyCodePoint != U_SENTINEL &&
      containsEmojiCharacters(firstMissingCharacter);
  const bool firstMissingIsCjk = firstMissingAnyCodePoint != U_SENTINEL &&
      containsCjkCharacters(firstMissingCharacter);
  if (firstMissingIsEmoji ||
      (!preferredAvailable && containsEmojiCharacters(sampleText) &&
       firstMissingCodePoint == U_SENTINEL)) {
   const QString emojiFallback = firstAvailableFamily(emojiFallbackCandidates());
   if (!emojiFallback.isEmpty()) {
    return emojiFallback;
   }
  }

  if (firstMissingIsCjk ||
      (!preferredAvailable && containsCjkCharacters(sampleText) &&
       firstMissingCodePoint == U_SENTINEL)) {
   const QString japaneseFallback = firstAvailableFamily(japaneseFallbackCandidates());
   if (!japaneseFallback.isEmpty()) {
    auto* tracker = FallbackTracker::instance();
    tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                     FallbackAction::Fallback, preferred, japaneseFallback,
                     "[FontManager] fallback family for CJK text", true});
    return japaneseFallback;
   }
  }

  if (firstMissingCodePoint != U_SENTINEL) {
   const QStringList candidates = scriptFallbackCandidates(firstMissingCodePoint);
   const QStringList families = availableFamilies();
   for (const QString& candidate : candidates) {
    if (!families.contains(candidate, Qt::CaseInsensitive)) continue;
    QFont fallbackFont(candidate);
    const QRawFont rawFallback = QRawFont::fromFont(fallbackFont, QFontDatabase::Any);
    if (!rawFontSupportsText(rawFallback, sampleText)) continue;
    if (shouldRecordScriptFallback(firstMissingCodePoint)) {
     auto* tracker = FallbackTracker::instance();
     tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                      FallbackAction::Fallback, preferred, candidate,
                      QStringLiteral("[FontManager] fallback family for script %1")
                          .arg(scriptTagForCodePoint(firstMissingCodePoint)),
                      true});
    }
    return candidate;
   }
  }

  return resolvedFamily(preferredFamily);
 }

 static bool loadFontFromFile(const QString& fontPath)
 {
  if (fontPath.isEmpty()) return false;
  int id = QFontDatabase::addApplicationFont(fontPath);
  if (id == -1) return false;
  registerApplicationFontFile(id, fontPath);
  fontDatabaseRevisionStorage().fetch_add(1, std::memory_order_relaxed);
  return true;
 }

 static std::atomic<std::uint64_t>& fontDatabaseRevisionStorage()
 {
  static std::atomic<std::uint64_t> revision{0};
  return revision;
 }

 static std::uint64_t fontDatabaseRevision()
 {
  return fontDatabaseRevisionStorage().load(std::memory_order_relaxed);
 }

 // Resolves the on-disk font file for a family so non-Qt consumers (for
 // example the HarfBuzz shaping backend) can obtain raw sfnt bytes.  Qt 6
 // exposes no whole-file accessor (QRawFont::fontTable returns a single
 // OpenType table), so the mapping is built from what Qt already reports.
 static std::optional<QByteArray> fontFileBytes(const QString& family,
                                                const QString& style = QString())
 {
  if (family.trimmed().isEmpty()) return std::nullopt;
  if (const QString path = applicationFontPathFor(family, style);
      !path.isEmpty()) {
   QFile file(path);
   if (file.open(QIODevice::ReadOnly)) return file.readAll();
  }
  if (const QString path = systemFontPathFor(family, style);
      !path.isEmpty()) {
   QFile file(path);
   if (file.open(QIODevice::ReadOnly)) return file.readAll();
  }
  return std::nullopt;
 }

 static QFont makeFont(const TextStyle& style, const QString& sampleText = QString())
 {  QFont font(resolvedFamilyForText(style.fontFamily.toQString(), sampleText));
  font.setPointSizeF(std::max(1.0f, style.fontSize));
  const int numericWeight = std::clamp(style.fontWeightValue, 0, 900);
  if (numericWeight >= 100) {
   font.setWeight(static_cast<QFont::Weight>(numericWeight));
  } else {
   font.setWeight(style.fontWeight == FontWeight::Bold ? QFont::Bold : QFont::Normal);
  }
  font.setItalic(style.fontStyle == FontStyle::Italic);
  font.setUnderline(style.underline);
  font.setStrikeOut(style.strikethrough);
  font.setCapitalization(style.allCaps ? QFont::AllUppercase : QFont::MixedCase);
  font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking);
  font.setStretch(std::clamp(static_cast<int>(std::lround(style.fontStretch)), 50, 200));
  return font;
 }

 private:
 // Family (lower case) -> font file path, populated from addApplicationFont.
 static std::unordered_map<QString, QString>& applicationFontFiles()
 {
  static std::unordered_map<QString, QString> files;
  return files;
 }

 static QString systemFontDirectory()
 {
 #ifdef Q_OS_WIN
  return QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts");
 #else
  const QStringList candidates{
      QStringLiteral("/usr/share/fonts"),
      QStringLiteral("/usr/local/share/fonts"),
      QStringLiteral("/Library/Fonts"),
      QStringLiteral("/System/Library/Fonts")};
  for (const QString& candidate : candidates) {
   if (QDir(candidate).exists()) return candidate;
  }
  return {};
 #endif
 }

 static QString fileNameForFamily(const QString& family, const QString& style)
 {
  QString base = family;
  if (!style.isEmpty() && style.compare(QStringLiteral("Regular"), Qt::CaseInsensitive) != 0) {
   base += QStringLiteral(" ") + style;
  }
  QString fileName = base;
  fileName.replace(QLatin1Char(' '), QLatin1Char('-'));
  return fileName;
 }

 static void registerApplicationFontFile(int id, const QString& path)
 {
  const QStringList families = QFontDatabase::applicationFontFamilies(id);
  if (families.isEmpty()) return;
  auto& files = applicationFontFiles();
  for (const QString& family : families) {
   files.emplace(family.toLower(), path);
  }
 }

 static QString applicationFontPathFor(const QString& family, const QString& style)
 {
  Q_UNUSED(style);
  auto& files = applicationFontFiles();
  const auto exact = files.find(family.toLower());
  if (exact != files.end()) return exact->second;
  return {};
 }

 static QString systemFontPathFor(const QString& family, const QString& style)
 {
  const QString directory = systemFontDirectory();
  if (directory.isEmpty()) return {};
  QDir dir(directory);
  if (!dir.exists()) return {};
  const QStringList nameFilters{QStringLiteral("*.ttf"), QStringLiteral("*.otf"),
                                QStringLiteral("*.ttc"), QStringLiteral("*.otc")};
  const QString wanted = fileNameForFamily(family, style).toLower();
  const QString familyLower = family.toLower();
  for (const QString& filter : nameFilters) {
   const QStringList files = dir.entryList({filter}, QDir::Files, QDir::Name);
   for (const QString& file : files) {
    if (file.toLower() == wanted + QStringLiteral(".ttf") ||
        file.toLower() == wanted + QStringLiteral(".otf") ||
        file.toLower() == wanted + QStringLiteral(".ttc") ||
        file.toLower() == wanted + QStringLiteral(".otc")) {
     return dir.filePath(file);
    }
   }
   for (const QString& file : files) {
    if (file.toLower().contains(familyLower)) return dir.filePath(file);
   }
  }
  return {};
 }

};

}
