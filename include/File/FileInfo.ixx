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

 public:
  FileInfo();
  ~FileInfo();
 };




};