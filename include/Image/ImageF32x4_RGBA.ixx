module;


#include <opencv2/opencv.hpp>
#include "../Define/DllExportMacro.hpp"
#include <QUuid>
#include <QObject>

#include <folly/concurrency/AtomicSharedPtr.h>


export module Image.ImageF32x4_RGBA;

import std;
import Size;
import FloatRGBA;


export namespace ArtifactCore {



  class LIBRARY_DLL_API ImageF32x4_RGBA :public QObject{
 public:
  ImageF32x4_RGBA();
  explicit ImageF32x4_RGBA(const FloatRGBA& color);
  ImageF32x4_RGBA(const ImageF32x4_RGBA& image);
  ~ImageF32x4_RGBA();
  auto toCVMat() const -> class cv::Mat;
  void fill(const FloatRGBA& rgba);
  void fillAlpha(float alpha=1.0f);
  int width() const;
  int height() const;
  void resize(int width,int height);

  ImageF32x4_RGBA DeepCopy()  const;

  ImageF32x4_RGBA createMaskLike(const ImageF32x4_RGBA& src, const FloatRGBA& fillColor);

  ImageF32x4_RGBA& operator=(const ImageF32x4_RGBA& other);
  ImageF32x4_RGBA(ImageF32x4_RGBA&&) noexcept = default;
  ImageF32x4_RGBA& operator=(ImageF32x4_RGBA&&) noexcept = default;
 private:
  class Impl;
  Impl* impl_;
 };

  //typedef std::shared_ptr<

 // ダミーのcv::Matラッパー型を forward declare（export してない）
 class cv_Mat;
}