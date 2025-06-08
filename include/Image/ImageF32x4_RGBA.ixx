module;

#include <memory>
#include <opencv2/core.hpp>
#include "../Define/DllExportMacro.hpp"


//#include <qt>


export module Image:ImageF32x4_RGBA;

import Size;
import FloatRGBA;


export namespace ArtifactCore {


 LIBRARY_DLL_API class ImageF32x4_RGBA {
 public:
  ImageF32x4_RGBA();
  explicit ImageF32x4_RGBA(const FloatRGBA& color);
  ~ImageF32x4_RGBA();

  // コピー返しにすることで、cv::Matのinclude不要
  auto toCVMat() const -> class cv_Mat;
  void fill(const FloatRGBA& rgba);
  int width() const;
  int height() const;
  void resize(int width,int height);

  ImageF32x4_RGBA DeepCopy()  const;

  ImageF32x4_RGBA createMaskLike(const ImageF32x4_RGBA& src, const FloatRGBA& fillColor);

  ImageF32x4_RGBA& operator=(const ImageF32x4_RGBA& other);
  ImageF32x4_RGBA(ImageF32x4_RGBA&&) noexcept = default;
  ImageF32x4_RGBA& operator=(ImageF32x4_RGBA&&) noexcept = default;
 private:
  struct Impl;                // ★ これが必要！！
  std::unique_ptr<Impl> impl_;
 };

 // ダミーのcv::Matラッパー型を forward declare（export してない）
 class cv_Mat;
}