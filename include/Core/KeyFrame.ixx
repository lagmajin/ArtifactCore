module;

#include <QString>

#include "../Define/DllExportMacro.hpp"
export module Core.KeyFrame;

import std;

export namespace Artifact {

 //keyframe class
 LIBRARY_DLL_API class KeyFrame
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