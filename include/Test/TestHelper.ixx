module;
#include <QString>
#include <QDirIterator>
#include <QCoreApplication>
#include "..\Define\DllExportMacro.hpp"
export module Test.Helper;

export namespace ArtifactCore {

 QString LIBRARY_DLL_API findFirstFileByLooseExtension(const QString& baseDir, const QString& extInput) {
  QString cleaned = extInput.trimmed();

  // .mp4, mp4, *mp4, *.mp4 → mp4
  if (cleaned.startsWith("*.")) {
   cleaned = cleaned.mid(2);
  }
  else {
   if (cleaned.startsWith("*")) cleaned = cleaned.mid(1);
   if (cleaned.startsWith(".")) cleaned = cleaned.mid(1);
  }

  if (cleaned.isEmpty())
   return QString();

  QString pattern = "*." + cleaned;

  QDirIterator it(baseDir, QStringList() << pattern, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
   QString foundPath = it.next();
   qDebug() << "Found file:" << foundPath;

   if (QFile::exists(foundPath)) {
	qDebug() << "✓ File exists.";
	return foundPath;
   }
   else {
	qDebug() << "✗ File listed but does not exist.";
   }
  }

  qDebug() << "No matching file found.";
  return QString();
 }

 QString LIBRARY_DLL_API findFirstFileByLooseExtensionFromAppDir(const QString& extInput) {
  QString appDir = QCoreApplication::applicationDirPath();
  return findFirstFileByLooseExtension(appDir, extInput);
 }


};