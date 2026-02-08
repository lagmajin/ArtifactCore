module ;
#include <mfidl.h>

#include "../Define/DllExportMacro.hpp"
export module Animation.Transform3D;

import std;
import Animation.Value;
import Animation.Transform2D;
import Time.Rational;
import Property.Abstract;

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
  void setInitialScale(const RationalTime& time,float xs,float ys);
  void setInitalAngle(const RationalTime& time,float angle=0);
  void setUserScale(const RationalTime& time,float xs,float ys);
  void setInitialPosition(const RationalTime& time,float px,float py);
  void setPosition(const RationalTime& time,float x, float y);
  void setRotation(const RationalTime& time,float degrees);
 	
  size_t size() const;

  float positionX() const;
  float positionY() const;
  float rotation() const;
  float scaleX() const;
  float scaleY() const;
 };

 };