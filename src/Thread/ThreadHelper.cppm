module;
#include "../../include/Define/DllExportMacro.hpp"
module Thread.Helper;

import std;


namespace ArtifactCore
{

void setCurrentThreadName(const std::string& name)
 {
  // Intentionally a no-op in the module build path.
  static_cast<void>(name);
 }

};
