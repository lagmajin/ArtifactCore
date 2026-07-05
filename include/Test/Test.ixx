module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QString>
export module Core.Test;



export import Test.Helper;

export namespace ArtifactCore {

 LIBRARY_DLL_API QString getDesktopFilePath(const QString& fileName) {
  QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  return QDir(desktopPath).filePath(fileName);
 }


}
