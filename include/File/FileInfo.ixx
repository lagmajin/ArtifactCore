module;
#include <utility>

export module File:Info;
#include <QtCore/QFile>


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
