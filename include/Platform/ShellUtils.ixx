module;
#include <utility>
#include <QString>
export module Platform.ShellUtils;

export namespace ArtifactCore {

 //class QString;

 class FileOpener {
 public:
  static void showInFolder(const QString& path, bool highlight = true);
 };



}
