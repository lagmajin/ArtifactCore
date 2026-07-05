module;
#include <utility>

#include <QString>
#include <QStringView>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <filesystem>

export module Utils.Path;

import Utils.String.Like;

export namespace ArtifactCore
{

 inline QString getAppPath()
 {
  return QCoreApplication::applicationDirPath();
 }

 // 実行ファイルのあるディレクトリ
 inline QString getExeDir()
 {
  return QCoreApplication::applicationDirPath();
 }

 // カレントディレクトリ
 inline QString getCurrentDir()
 {
  return QDir::currentPath();
 }

 inline QString getIconPath()
 {
  QDir dir(getAppPath());
  return dir.filePath("Icon");
 }

 inline QString getIconResourceRoot()
 {
  return QStringLiteral(":/icons");
 }

 inline QString normalizeIconResourceName(const QString& fileName)
 {
  QString normalized = fileName;
  normalized.replace('\\', '/');
  while (normalized.startsWith('/')) {
   normalized.remove(0, 1);
  }
  return normalized;
 }

 inline QString resolveIconPath(const QString& fileName)
 {
  QDir dir(getAppPath());
  dir.cd("Icon"); // getAppPath()/Icon に移動
  const QString filePath = dir.filePath(fileName);
  if (QFileInfo::exists(filePath)) {
   return filePath;
  }
  return getIconResourceRoot() + "/" + normalizeIconResourceName(fileName);
 }

 inline QString resolveIconResourcePath(const QString& fileName)
 {
  return getIconResourceRoot() + "/" + normalizeIconResourceName(fileName);
 }

 inline std::filesystem::path toStdPath(const QString& path)
 {
  return std::filesystem::path(path.toStdWString());
 }

 inline std::filesystem::path toStdPath(QStringView path)
 {
  return std::filesystem::path(path.toString().toStdWString());
 }

 inline QString toQString(const std::filesystem::path& path)
 {
  return QString::fromStdWString(path.wstring());
 }

 using PathView = QStringView;

 inline QString toQString(PathView path)
 {
  return path.toString();
 }

 inline QString joinPath(const QString& base, const QString& child)
 {
  QDir dir(base);
  return dir.filePath(child);
 }

 inline QString parentPath(const QString& path)
 {
  return QFileInfo(path).dir().absolutePath();
 }

 inline QString dirName(const QString& path)
 {
  return parentPath(path);
 }

 inline QString fileName(const QString& path)
 {
  return QFileInfo(path).fileName();
 }

 inline QString baseName(const QString& path)
 {
  return QFileInfo(path).baseName();
 }

 inline QString completeBaseName(const QString& path)
 {
  return QFileInfo(path).completeBaseName();
 }

 inline QString suffix(const QString& path)
 {
  return QFileInfo(path).suffix();
 }

 inline bool exists(const QString& path)
 {
  return QFileInfo::exists(path);
 }

 inline bool isAbsolute(const QString& path)
 {
  return QFileInfo(path).isAbsolute();
 }

 inline QString absolutePath(const QString& path)
 {
  return QFileInfo(path).absoluteFilePath();
 }

 inline QString relativePath(const QString& base, const QString& target)
 {
  return QDir(base).relativeFilePath(target);
 }

 inline QString completeSuffix(const QString& path)
 {
  return QFileInfo(path).completeSuffix();
 }

 inline QString pathRoot(const QString& path)
 {
  return QFileInfo(path).absolutePath();
 }

 inline QString rootPath(const QString& path)
 {
  return pathRoot(path);
 }

 inline QString parentDir(const QString& path)
 {
  return parentPath(path);
 }

 inline QString normalizedPath(const QString& path)
 {
  return QDir::cleanPath(path);
 }

 inline bool isRelative(const QString& path)
 {
  return QFileInfo(path).isRelative();
 }

 inline bool pathExists(const QString& path)
 {
  return exists(path);
 }

 inline bool pathIsRelative(const QString& path)
 {
  return isRelative(path);
 }

 inline bool pathIsAbsolute(const QString& path)
 {
  return isAbsolute(path);
 }

 inline QString cleanPath(const QString& path)
 {
  return normalizedPath(path);
 }

 inline QString normalizePath(const QString& path)
 {
  return normalizedPath(path);
 }

 inline QString lastSegment(const QString& path)
 {
  return fileName(path);
 }

 inline QString leafName(const QString& path)
 {
  return fileName(path);
 }

 inline QString terminalName(const QString& path)
 {
  return fileName(path);
 }

 inline QString fileStem(const QString& path)
 {
  return baseName(path);
 }

 inline QString stemName(const QString& path)
 {
  return baseName(path);
 }

 inline QString baseSegment(const QString& path)
 {
  return fileName(path);
 }

 inline QString endSegment(const QString& path)
 {
  return fileName(path);
 }

 inline QString trailingName(const QString& path)
 {
  return fileName(path);
 }

 inline QString fileTitle(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString completeTitle(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString titleName(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString nameTitle(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString titlePath(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString pathTitle(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString titleSegment(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString headingName(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString headerName(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString headName(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString topName(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString rootName(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString baseTitle(const QString& path)
 {
  return completeBaseName(path);
 }

 inline QString titleBase(const QString& path)
 {
  return completeBaseName(path);
 }

 using PathString = QString;
 using AbsolutePath = QString;

 class Path {
 private:
  class Impl;
  Impl* impl_;
 public:
  Path();

  ~Path();
  Path(const Path&) = delete;
  Path& operator=(const Path&) = delete;
 };

 using RootPath = QString;
 using FileTitle = QString;
using BaseTitle = QString;
using TitleBase = QString;
using TitlePath = QString;
using PathTitle = QString;
using HeadTitle = QString;
using NameTitle = QString;
using TitleName = QString;
using TitleHead = QString;
using HeadName = QString;
using PathHead = QString;
using HeadPath = QString;
using NameHead = QString;
using HeadTitlePath = QString;
using TitleHeadPath = QString;
using PathTitleHead = QString;
using TitlePathHead = QString;
using PathNameHead = QString;
using NamePathHead = QString;
using PathNameTitle = QString;
using TitleNamePath = QString;
using NameTitlePath = QString;
using TitleNameHead = QString;
using HeadTitleName = QString;
using TitleHeadName = QString;
using HeadNameTitle = QString;
using NameHeadTitle = QString;
using TitleHeadPathName = QString;
using PathTitleHeadName = QString;
using HeadPathTitle = QString;



}
