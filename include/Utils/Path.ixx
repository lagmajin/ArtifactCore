module;
#include <utility>

#include <QString>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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



}
