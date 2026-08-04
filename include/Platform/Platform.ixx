module;
#include <utility>
export module Platform;

export import Platform.ShellUtils;
export import Platform.Power;
export import :Hint;



export namespace ArtifactCore {


 void allocConsole();
 // Backward-compatible spelling retained for existing callers.
 void allocConsle();




}
