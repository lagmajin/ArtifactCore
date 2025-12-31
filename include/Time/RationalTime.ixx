module;
#include <QTime>
#include "../Define/DllExportMacro.hpp"
export module RationalTime;

import std;

export namespace ArtifactCore {

 class LIBRARY_DLL_API RationalTime {
 private:
  class Impl;
  Impl* impl_;
 public:
  RationalTime();
  ~RationalTime();
 };


};