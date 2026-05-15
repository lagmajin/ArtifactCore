module;
#include <QString>

export module File.TypeDetector;

export namespace ArtifactCore {

 enum class FileType {
  Unknown,
  Image,
  Video,
  Audio,
  Text,
  Binary,
  Document,
  Archive,
  Model3D
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
