module;
#include <utility>
#include "../Define/DllExportMacro.hpp"

export module Core.Test;
#include <QStandardPaths>
#include <QDir>
#include <QString>

export import Test.Helper;

export namespace ArtifactCore {

 LIBRARY_DLL_API QString getDesktopFilePath(const QString& fileName) {
  QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  return QDir(desktopPath).filePath(fileName);
 }


}
