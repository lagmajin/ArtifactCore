
module;
#include "../Define/DllExportMacro.hpp"
#include <QObject>
export module Image:ImageF32x4_With_Cache;

import std;
import Image;

export namespace ArtifactCore
{
 class LIBRARY_DLL_API ImageF32x4RGBAWithCache:public QObject{
 private:
  class Impl;

 public:
  ImageF32x4RGBAWithCache();
  explicit ImageF32x4RGBAWithCache(const ImageF32x4_RGBA& image);
 ~ImageF32x4RGBAWithCache();
  void UpdateGpuTextureFromCpuData();
 	void UpdateCpuDataFromGpuTexture();

 };
















};