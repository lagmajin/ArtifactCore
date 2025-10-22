module;

#include <QString>

#include "../Define/DllExportMacro.hpp"
export module Core.KeyFrame;

import std;

export namespace Artifact {

 //keyframe class
 class  LIBRARY_DLL_API KeyFrame
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  KeyFrame();
  ~KeyFrame();
  void setTime(float t);
  float time() const;
  void setValue(const std::any& v);

 };










};