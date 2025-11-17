module;

#include <QString>
#include <QCoreApplication>
#include <QDir>

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

 inline QString resolveIconPath(const QString& fileName)
 {
  QDir dir(getAppPath());
  dir.cd("Icon"); // getAppPath()/Icon に移動
  return dir.filePath(fileName);
 }

 class Path {
 private:
  class Impl;
  Impl* impl_;
 public:
  Path();

  ~Path();
 };



}