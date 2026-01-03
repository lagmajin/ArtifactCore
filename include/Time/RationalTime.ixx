module;
#include <QTime>
#include "../Define/DllExportMacro.hpp"
export module Time.Rational;

import std;

export namespace ArtifactCore {

 class LIBRARY_DLL_API RationalTime {
 private:
  class Impl;
  Impl* impl_;
 public:
  RationalTime();
  RationalTime(int64_t value,int64_t scale);
  ~RationalTime();

 };


};