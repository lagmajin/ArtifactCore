module;
#include <QString>

export module Platform.ShellUtils;

namespace ArtifactCore {

 //class QString;

 class FileOpener {
 public:
  static void showInFolder(const QString& path, bool highlight = true);
 };



}