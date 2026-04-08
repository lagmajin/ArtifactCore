module;
#include <utility>

#include <vector>
#include <algorithm>

#include <QString>
#include <QStringList>
#include <QFont>
#include <QFontDatabase>

export module Font.FreeFont;

import Text.Style;
import Font.Descriptor;

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
   return {};
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

 static QString resolvedFamily(const QString& preferredFamily)
 {
  const QString preferred = preferredFamily.trimmed();
  if (!preferred.isEmpty() && isFamilyAvailable(preferred)) {
   return preferred;
  }

  const QString general = defaultSansSerifFamily();
  if (!general.isEmpty()) {
   return general;
  }

  const QString japaneseFallback = firstAvailableFamily(japaneseFallbackCandidates());
  if (!japaneseFallback.isEmpty()) {
   return japaneseFallback;
  }

  const QStringList families = availableFamilies();
  if (!families.isEmpty()) {
   return families.front();
  }

  return QStringLiteral("Arial");
 }

 static QString resolvedFamilyForText(const QString& preferredFamily, const QString& sampleText)
 {
  const QString preferred = preferredFamily.trimmed();
  if (!preferred.isEmpty() && isFamilyAvailable(preferred)) {
   return preferred;
  }

  if (containsCjkCharacters(sampleText)) {
   const QString japaneseFallback = firstAvailableFamily(japaneseFallbackCandidates());
   if (!japaneseFallback.isEmpty()) {
    return japaneseFallback;
   }
  }

  return resolvedFamily(preferredFamily);
 }

 static bool loadFontFromFile(const QString& fontPath)
 {
  if (fontPath.isEmpty()) return false;
  int id = QFontDatabase::addApplicationFont(fontPath);
  return id != -1;
 }

 static QFont makeFont(const TextStyle& style, const QString& sampleText = QString())
 {  QFont font(resolvedFamilyForText(style.fontFamily.toQString(), sampleText));
  font.setPointSizeF(std::max(1.0f, style.fontSize));
  font.setBold(style.fontWeight == FontWeight::Bold);
  font.setItalic(style.fontStyle == FontStyle::Italic);
  font.setUnderline(style.underline);
  font.setStrikeOut(style.strikethrough);
  font.setCapitalization(style.allCaps ? QFont::AllUppercase : QFont::MixedCase);
  font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking);
  return font;
 }
};

}
