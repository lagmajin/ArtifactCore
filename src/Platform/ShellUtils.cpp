module;
#include <QString>

#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QDesktopServices>
#include <QDir>
//#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#include <windows.h>
#endif
module Platform:ShellUtils;

namespace ArtifactCore {



 void FileOpener::showInFolder(const QString& path, bool highlight) {
  QFileInfo info(path);
  if (!info.exists()) return;

#ifdef _WIN32
  QString param;
  if (highlight) {
   param = "/select,\"" + QDir::toNativeSeparators(path) + "\"";
  }
  else {
   param = "\"" + QDir::toNativeSeparators(info.absolutePath()) + "\"";
  }
  ::ShellExecuteW(nullptr, L"open", L"explorer", (LPCWSTR)param.utf16(), nullptr, SW_SHOWNORMAL);

#elif defined(__APPLE__)
  QStringList args;
  if (highlight) {
   args << "-R" << path;
  }
  else {
   args << path;
  }
  QProcess::startDetached("open", args);

#else  // Linux (freedesktopŒn)
  QString folderPath = highlight ? info.absoluteFilePath() : info.absolutePath();
  QUrl url = QUrl::fromLocalFile(folderPath);
  QDesktopServices::openUrl(url);
#endif
 }

}
