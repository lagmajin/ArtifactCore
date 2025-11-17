module;


export module Platform.ShellUtils;


import <QString>; 

namespace ArtifactCore {

 //class QString;

 class FileOpener {
 public:
  static void showInFolder(const QString& path, bool highlight = true);
 };



}