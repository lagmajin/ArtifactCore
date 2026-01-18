module;
#include <QString>
#include <QByteArray>

export module File.TypeDetector;


import std;

export namespace ArtifactCore {

 enum class FileType {
  Unknown,
  Image,
  Video,
  Audio,
  Text,
  Binary,
  Document,
  Archive
 };

 class FileTypeDetector {
 private:
  class Impl;
  Impl* impl_;
 public:
  FileTypeDetector();
  ~FileTypeDetector();

  FileType detect(const QString& filePath) const;
  FileType detectByExtension(const QString& filePath) const;
  FileType detectByMagicNumber(const QString& filePath) const;
 };

}