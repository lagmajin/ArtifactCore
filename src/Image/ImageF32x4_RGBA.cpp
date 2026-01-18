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
  Impl(const FloatRGBA& rgba, int width = 1, int height = 1); // Add size parameters
  Impl(const Impl& other); // Copy constructor
  Impl& operator=(const Impl& other); // Copy assignment
 };

 int32_t ImageF32x4_RGBA::Impl::width() const
 {
  return static_cast<int32_t>(mat_.cols);
 }

 int32_t ImageF32x4_RGBA::Impl::height() const
 {
  return static_cast<int32_t>(mat_.rows);
 }

 ImageF32x4_RGBA::Impl::Impl(const FloatRGBA& rgba, int width, int height)
  : mat_(height, width, CV_32FC4)
 {
  cv::Vec4f color(rgba.r(), rgba.g(), rgba.b(), rgba.a()); // Assuming FloatRGBA is RGBA
  mat_.setTo(color);
 }

 ImageF32x4_RGBA::Impl::Impl()
  : mat_(1, 1, CV_32FC4, cv::Scalar(0, 0, 0, 1)) // Default 1x1 with transparent
 {

 }

 ImageF32x4_RGBA::Impl::Impl(const Impl& other)
  : mat_(other.mat_.clone())
 {

 }

 ImageF32x4_RGBA::Impl& ImageF32x4_RGBA::Impl::operator=(const Impl& other)
 {
  if (this != &other) {
    mat_ = other.mat_.clone();
  }
  return *this;
 }

 ImageF32x4_RGBA::ImageF32x4_RGBA()
  : impl_(new Impl())
 {

 }

 ImageF32x4_RGBA::ImageF32x4_RGBA(const FloatRGBA& color)
  : impl_(new Impl(color, 1, 1)) // Default size, can be resized later
 {

 }

 ImageF32x4_RGBA::ImageF32x4_RGBA(const ImageF32x4_RGBA& image)
  : impl_(new Impl(*image.impl_))
 {

 }

 void ImageF32x4_RGBA::fill(const FloatRGBA& rgba)
 {
  cv::Vec4f color(rgba.r(), rgba.g(), rgba.b(), rgba.a());
  impl_->mat_.setTo(color);
 }

 void ImageF32x4_RGBA::resize(int width, int height)
 {
  cv::Mat resized;
  cv::resize(impl_->mat_, resized, cv::Size(width, height));
  impl_->mat_ = resized;
 }


  ImageF32x4_RGBA ImageF32x4_RGBA::createMaskLike(const ImageF32x4_RGBA& src, const FloatRGBA& fillColor /*= FloatRGBA*/)
  {
   ImageF32x4_RGBA mask(fillColor);
   mask.resize(src.width(), src.height());
   return mask;
  }

  ImageF32x4_RGBA& ImageF32x4_RGBA::operator=(const ImageF32x4_RGBA& other)
  {
   if (this != &other) {
    *impl_ = *other.impl_;
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
   return impl_->mat_;
  }

  void ImageF32x4_RGBA::fillAlpha(float alpha/*=1.0f*/)
  {
   std::vector<cv::Mat> channels;
   cv::split(impl_->mat_, channels);
   if (channels.size() >= 4) {
     channels[3].setTo(alpha);
     cv::merge(channels, impl_->mat_);
   }
  }

  ImageF32x4_RGBA::~ImageF32x4_RGBA()
  {
   delete impl_;
  }

  ImageF32x4_RGBA ImageF32x4_RGBA::DeepCopy() const
  {
   ImageF32x4_RGBA copy;
   copy.impl_->mat_ = impl_->mat_.clone();
   return copy;
  }

};