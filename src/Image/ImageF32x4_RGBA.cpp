module ;
#include <QList>
#include <opencv2/opencv.hpp>


module Image.ImageF32x4_RGBA;

import std;
import FloatRGBA;

namespace ArtifactCore {

 class  ImageF32x4_RGBA::Impl
 {

 public:
  cv::Mat mat_;
  int32_t width() const;
  int32_t height() const;
  Impl();
  Impl(const FloatRGBA& rgba);
 };

 int32_t ImageF32x4_RGBA::Impl::width() const
 {
  return static_cast<int32_t>(mat_.cols);
 }

 int32_t ImageF32x4_RGBA::Impl::height() const
 {
  return static_cast<int32_t>(mat_.rows);
 }

 ImageF32x4_RGBA::Impl::Impl(const FloatRGBA& rgba)
 {
  
  cv::Vec4f color(rgba.b(), rgba.g(), rgba.r(), rgba.a());  

  mat_.setTo(color);
 }

 ImageF32x4_RGBA::Impl::Impl()
 {

 }

 ImageF32x4_RGBA::ImageF32x4_RGBA()
 {

 }

 ImageF32x4_RGBA::ImageF32x4_RGBA(const FloatRGBA& color):impl_(new Impl())
 {

 }

 ImageF32x4_RGBA::ImageF32x4_RGBA(const ImageF32x4_RGBA& image) :impl_(new Impl())
 {

 }

 void ImageF32x4_RGBA::fill(const FloatRGBA& rgba)
 {
 
   // OpenCVはBGR(A)の順を使うが、ここではFloatRGBAがRGBAと仮定
   cv::Vec4f color(rgba.b(), rgba.g(), rgba.r(), rgba.a()); // 要修正：順番次第では（r, g, b, a）→（b, g, r, a）
   //impl_->mat.setTo(color);
  
 }

 void ImageF32x4_RGBA::resize(int width, int height)
 {

 }


  ImageF32x4_RGBA ImageF32x4_RGBA::createMaskLike(const ImageF32x4_RGBA& src, const FloatRGBA& fillColor /*= FloatRGBA*/)
  {
   ImageF32x4_RGBA mask;
   mask = ImageF32x4_RGBA(); // デフォルトコンストラクタで初期化
   mask.resize(src.width(), src.height()); // implのresizeメソッドがあれば使う想定
   mask.fill(fillColor);
   return mask;
  }

  ImageF32x4_RGBA& ImageF32x4_RGBA::operator=(const ImageF32x4_RGBA& other)
  {
   if (this != &other) {

   	//if (other.impl_) {
	 //impl_ = std::make_unique<Impl>(*other.impl_);  // ★ Impl にコピーコンストラクタが必要！
	//}
	//else {
	 //impl_.reset();  // nullptr を代入
	//}
   }
   return *this;
  }



  int ImageF32x4_RGBA::width() const
  {
   return impl_->width();
  }

  int ImageF32x4_RGBA::height() const
  {
   return impl_->height();
  }

  cv::Mat ImageF32x4_RGBA::toCVMat()const
  {
   //return impl_->mat;
   return cv::Mat();
  }

  void ImageF32x4_RGBA::fillAlpha(float alpha/*=1.0f*/)
  {

  }

  ImageF32x4_RGBA::~ImageF32x4_RGBA()
  {
   delete impl_;
  }
 




};