module ;
#include "../Define/DllExportMacro.hpp"
export module Animation.Transform3D;

import std;
import Animation.Transform2D;

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
 };

 };