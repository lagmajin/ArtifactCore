#if defined(_WIN32)
#include <windows.h>
#endif




namespace ArtifactCore {



void allocConsole() {
#if defined(_WIN32)
  if (GetConsoleWindow() == nullptr) {
    AllocConsole();
  }
#endif
}

void allocConsle() { allocConsole(); }





}
