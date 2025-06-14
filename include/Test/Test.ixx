module;

#include <QStandardPaths>
#include <QDir>
#include <QString>
#include "../Define/DllExportMacro.hpp"

export module Test;



export namespace ArtifactCore {

 LIBRARY_DLL_API QString getDesktopFilePath(const QString& fileName) {
  QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  return QDir(desktopPath).filePath(fileName);
 }


}