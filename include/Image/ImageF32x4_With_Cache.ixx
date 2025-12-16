
module;
#include "../Define/DllExportMacro.hpp"
#include <QObject>
#include <wobjectdefs.h>
export module Image.ImageF32x4RGBAWithCache;

import std;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore
{
	class ImageF32x4_RGBA;


class LIBRARY_DLL_API ImageF32x4RGBAWithCache:public QObject{
    W_OBJECT(ImageF32x4RGBAWithCache)
 private:
  class Impl;
  Impl* impl_;
 public:
  ImageF32x4RGBAWithCache();
  explicit ImageF32x4RGBAWithCache(const ImageF32x4_RGBA& image);
  ~ImageF32x4RGBAWithCache();
  ImageF32x4_RGBA& image() const;

  void UpdateGpuTextureFromCpuData();
  void UpdateCpuDataFromGpuTexture();

  int32_t width() const;
  int32_t height() const;
  bool IsGpuTextureValid() const;


 };

typedef std::shared_ptr<ImageF32x4RGBAWithCache> ImageF32x4RGBAWithCachePtr;














};