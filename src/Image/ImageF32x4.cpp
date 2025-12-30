module;
#include <opencv2/opencv.hpp>
module ImageF32x4;


namespace ArtifactCore {

 struct ImageF32x4::Impl {
  cv::Mat mat;

  Impl(int w, int h) : mat(h, w, CV_32FC4, cv::Scalar(0, 0, 0, 0)) {}
 };

 ImageF32x4::ImageF32x4(int width, int height)
  : impl_(new Impl(width, height))
 {
 }

 ImageF32x4::ImageF32x4() :impl_(new Impl())
 {

 }

 ImageF32x4::~ImageF32x4()
 {
   delete impl_;
 }

 int ImageF32x4::width() const {
  return impl_->mat.cols;
 }

 int ImageF32x4::height() const {
  return impl_->mat.rows;
 }




}