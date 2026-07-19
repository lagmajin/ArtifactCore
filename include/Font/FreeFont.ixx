module;
#include <utility>

#include <vector>
#include <algorithm>
#include <cmath>

#include <QString>
#include <QStringList>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QDebug>
#include <QDateTime>

export module Font.FreeFont;

import Text.Style;
import Font.Descriptor;
import Core.Diagnostics.FallbackPolicy;

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
  const QString preferred = preferredFamily.trimmed();
   if (!preferred.isEmpty() && isFamilyAvailable(preferred)) {
   QFont preferredFont(preferred);
   const QRawFont rawFont = QRawFont::fromFont(preferredFont, QFontDatabase::Any);
   bool needsCjkFallback = false;
   for (const QChar ch : sampleText) {
    if (containsCjkCharacters(QString(ch)) && (!rawFont.isValid() || !rawFont.supportsCharacter(ch))) {
     needsCjkFallback = true;
     break;
    }
   }
   if (!needsCjkFallback) {
    return preferred;
   }
  }

  if (containsCjkCharacters(sampleText)) {
   const QString japaneseFallback = firstAvailableFamily(japaneseFallbackCandidates());
   if (!japaneseFallback.isEmpty()) {
    auto* tracker = FallbackTracker::instance();
    tracker->record({QDateTime::currentDateTime(), FallbackCategory::Font,
                     FallbackAction::Fallback, preferred, japaneseFallback,
                     "[FontManager] fallback family for CJK text", true});
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
  font.setStretch(std::clamp(static_cast<int>(std::lround(style.fontStretch)), 50, 200));
  return font;
 }
};

}
