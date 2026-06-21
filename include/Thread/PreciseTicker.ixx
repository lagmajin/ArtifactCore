module;

#include <atomic>
#include <chrono>
#include <functional>
#include "../Define/DllExportMacro.hpp"

export module Thread.PreciseTicker;

export namespace ArtifactCore {

class LIBRARY_DLL_API PreciseTicker {
public:
  using Duration = std::chrono::milliseconds;
  using Callback = std::function<void()>;

  PreciseTicker();
  ~PreciseTicker();

  PreciseTicker(const PreciseTicker&) = delete;
  PreciseTicker& operator=(const PreciseTicker&) = delete;

  void setInterval(Duration interval);
  Duration interval() const;

  void setCallback(Callback callback);

  void start();
  void stop();
  bool isRunning() const;

private:
  class Impl;
  Impl* impl_ = nullptr;
};

}
