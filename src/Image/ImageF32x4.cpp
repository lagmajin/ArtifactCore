
#include <opencv2/opencv.hpp>

import ImageF32x4;

namespace ArtifactCore {

 struct ImageF32x4::Impl {
  cv::Mat mat;

  Impl(int w, int h) : mat(h, w, CV_32FC4, cv::Scalar(0, 0, 0, 0)) {}
 };

 ImageF32x4::ImageF32x4(int width, int height)
  : pimpl_(std::make_unique<Impl>(width, height))
 {
 }

 ImageF32x4::ImageF32x4()
 {

 }

 ImageF32x4::~ImageF32x4() = default;

 int ImageF32x4::width() const {
  return pimpl_->mat.cols;
 }

 int ImageF32x4::height() const {
  return pimpl_->mat.rows;
 }




}