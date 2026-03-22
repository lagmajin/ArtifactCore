module;

#include <QString>
#include <QStringList>
#include <QFont>
#include <QFontDatabase>

export module Font.FreeFont;

import std;
import Text.Style;

export namespace ArtifactCore
{

class FontManager
{
public:
 static QStringList availableFamilies()
 {
  return QFontDatabase::families();
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

  const QStringList families = availableFamilies();
  if (!families.isEmpty()) {
   return families.front();
  }

  return QStringLiteral("Arial");
 }

 static QFont makeFont(const TextStyle& style)
 {
  QFont font(resolvedFamily(style.fontFamily.toQString()));
  font.setPointSizeF(std::max(1.0f, style.fontSize));
  font.setBold(style.bold);
  font.setItalic(style.italic);
  font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking);
  return font;
 }
};

}
