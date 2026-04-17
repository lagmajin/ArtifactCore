module;
#include "../../include/Define/DllExportMacro.hpp"
#include <QThread>
module Thread.Helper;

import std;


namespace ArtifactCore
{

namespace {
struct SharedBackgroundThreadPoolHolder {
  SharedBackgroundThreadPoolHolder() {
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    pool.setMaxThreadCount(std::max(1, hw > 0 ? std::min(hw, 4) : 2));
    pool.setExpiryTimeout(-1);
  }

  QThreadPool pool;
};
} // namespace

QThreadPool& sharedBackgroundThreadPool()
{
  static SharedBackgroundThreadPoolHolder holder;
  return holder.pool;
}

void setCurrentThreadName(const std::string& name)
 {
  // Intentionally a no-op in the module build path.
  static_cast<void>(name);
 }

};
