module;
#include <algorithm>
#include <utility>
#include <memory>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

module Layer2D;

import Image;
import Layer.Blend;
import Layer.Matte;
import Memory.TrackedPtr;

namespace ArtifactCore {

 struct Layer2D::Impl
 {
  ImageF32x4 image;
  ImageF32x4_RGBA rgbaImage;
  StaticTransform2D transform;
  Layer2DSetting settings;  // opacity, blendMode, matteMode
 };


 Layer2D::Layer2D() : impl_(std::make_unique<Impl>())
 {
  impl_->settings.opacity = 1.0f;
  impl_->settings.blendMode = BlendMode::Normal;
  impl_->settings.matteMode = MatteMode::None;
 }

 Layer2D::~Layer2D()
 {

 }

 StaticTransform2D Layer2D::transform2D() const
 {
  return impl_->transform;
 }

 void Layer2D::setTransform2D(const StaticTransform2D& t)
 {
  impl_->transform = t;
 }
 
 ImageF32x4_RGBA Layer2D::transformedLayer()
 {
  if (impl_->rgbaImage.isEmpty())
   return ImageF32x4_RGBA();

  cv::Mat src = impl_->rgbaImage.toCVMat();
  if (src.empty())
   return impl_->rgbaImage;

  QTransform qt = impl_->transform.toQTransform();
  bool hasTransform = !qt.isIdentity();
  if (!hasTransform)
   return impl_->rgbaImage;

  // Build 2x3 affine matrix from QTransform
  cv::Mat affine(2, 3, CV_64FC1);
  affine.at<double>(0, 0) = qt.m11();
  affine.at<double>(0, 1) = qt.m21();
  affine.at<double>(0, 2) = qt.m31();  // dx
  affine.at<double>(1, 0) = qt.m12();
  affine.at<double>(1, 1) = qt.m22();
  affine.at<double>(1, 2) = qt.m32();  // dy

  cv::Mat dst;
  cv::warpAffine(src, dst, affine, src.size(),
   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));

  ImageF32x4_RGBA result;
  result.setFromCVMat(dst);
  return result;
 }

 // Opacity 
 float Layer2D::opacity() const
 {
  return impl_->settings.opacity;
 }
 
 void Layer2D::setOpacity(float value)
 {
  impl_->settings.opacity = std::clamp(value, 0.0f, 1.0f);
 }
 
 // Blend/Matte 
 BlendMode Layer2D::blendMode() const
 {
  return impl_->settings.blendMode;
 }
 
 void Layer2D::setBlendMode(BlendMode mode)
 {
  impl_->settings.blendMode = mode;
 }
 
 MatteMode Layer2D::matteMode() const
 {
  return impl_->settings.matteMode;
 }
 
  void Layer2D::setMatteMode(MatteMode mode)
  {
   impl_->settings.matteMode = mode;
  }

  const MatteStack& Layer2D::matteStack() const
  {
   return impl_->settings.matteStack;
  }

  void Layer2D::setMatteStack(const MatteStack& stack)
  {
   impl_->settings.matteStack = stack;
  }

  void Layer2D::setImage(const ImageF32x4_RGBA& image)
  {
   impl_->rgbaImage = image;
  }

  ImageF32x4_RGBA Layer2D::image() const
  {
   return impl_->rgbaImage;
  }

}