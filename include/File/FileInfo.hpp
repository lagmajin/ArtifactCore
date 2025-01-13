#pragma once

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

 public:
  FileInfo();
  ~FileInfo();
 };




};