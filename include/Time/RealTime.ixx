module;
#include "../Define/DllExportMacro.hpp"
export module Time.Real;



export namespace ArtifactCore
{

 class  LIBRARY_DLL_API RealTime {
 private:
  class Impl;
  Impl* impl_;

 public:
  RealTime();
  ~RealTime();

  void update(double newTime);

  void pause();
  void resume();
  void setTimeScale(double scale);

  double getCurrentTime() const;
  double getDeltaTime() const;
  bool isPaused() const;
 };




}