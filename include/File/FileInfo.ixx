module;

#include <QtCore/QFile>

export module File:Info;


namespace ArtifactCore {

 enum eFileType {
  Unknown,
  Image,
  Video,
  Audio,
 };

 class FileInfo {
 private:
  class Impl;
  Impl* impl_;
 public:
  FileInfo();
  ~FileInfo();
 };




};