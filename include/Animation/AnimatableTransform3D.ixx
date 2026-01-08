module ;
#include <mfidl.h>

#include "../Define/DllExportMacro.hpp"
export module Animation.Transform3D;

import std;
import Animation.Value;
import Animation.Transform2D;
import Time.Rational;


export namespace ArtifactCore
{

 class LIBRARY_DLL_API AnimatableTransform3D
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  AnimatableTransform3D();
  ~AnimatableTransform3D();
  void setInitialScale(float xs,float ys);
 	
  void setUserScale(const RationalTime& time,float xs,float ys);
  void setPosition(const RationalTime& time,float x, float y);
  void setRotation(const RationalTime& time,float degrees);
  void setScale(float sx, float sy);
  size_t size() const;
 };

 };