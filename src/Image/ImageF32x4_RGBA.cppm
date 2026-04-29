module;


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QString>
#include <QList>
#include <QFileInfo>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>

module Image.ImageF32x4_RGBA;




import FloatRGBA;
import CvUtils;

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
  
  // ピクセルアクセス
  FloatRGBA getPixel(int x, int y) const;
  void setPixel(int x, int y, const FloatRGBA& color);
  
  // 画像情報
  bool isEmpty() const;
  size_t totalPixels() const;
 };

 int32_t ImageF32x4_RGBA::Impl::width() const
 {
  return static_cast<int32_t>(mat_.cols);
 }

 int32_t ImageF32x4_RGBA::Impl::height() const
 {
  return static_cast<int32_t>(mat_.rows);
 }

 bool ImageF32x4_RGBA::Impl::isEmpty() const
 {
  return mat_.empty();
 }

 size_t ImageF32x4_RGBA::Impl::totalPixels() const
 {
  return static_cast<size_t>(mat_.rows * mat_.cols);
 }

 FloatRGBA ImageF32x4_RGBA::Impl::getPixel(int x, int y) const
 {
  if (x < 0 || x >= mat_.cols || y < 0 || y >= mat_.rows) {
   return FloatRGBA(); // 範囲外は透明を返す
  }
  
  cv::Vec4f pixel = mat_.at<cv::Vec4f>(y, x);
  return FloatRGBA(pixel[0], pixel[1], pixel[2], pixel[3]);
 }

 void ImageF32x4_RGBA::Impl::setPixel(int x, int y, const FloatRGBA& color)
 {
  if (x >= 0 && x < mat_.cols && y >= 0 && y < mat_.rows) {
   mat_.at<cv::Vec4f>(y, x) = cv::Vec4f(color.r(), color.g(), color.b(), color.a());
  }
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

 bool ImageF32x4_RGBA::load(const QString& path)
 {
  const std::string utf8Path = path.toUtf8().toStdString();
  OIIO::ImageBuf source(utf8Path);
  if (source.read(0, 0, true, OIIO::TypeDesc::FLOAT)) {
    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    if (spec.width > 0 && spec.height > 0 && spec.nchannels > 0) {
      OIIO::ImageBuf rgba;
      if (spec.nchannels >= 4) {
        const std::array<int, 4> channelOrder{0, 1, 2, 3};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);
      } else if (spec.nchannels == 3) {
        const std::array<int, 4> channelOrder{0, 1, 2, -1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
      } else if (spec.nchannels == 2) {
        const std::array<int, 4> channelOrder{0, 0, 0, 1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
      } else {
        const std::array<int, 4> channelOrder{0, 0, 0, -1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
      }

      cv::Mat mat(spec.height, spec.width, CV_32FC4);
      if (rgba.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT, mat.ptr<float>())) {
        setFromCVMat(mat);
        return true;
      }
    }
  }

  QImage img(path);
  if (img.isNull()) return false;
  setFromCVMat(CvUtils::qImageToCvMat(img));
  return true;
 }

 bool ImageF32x4_RGBA::save(const QString& path) const
 {
  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == QStringLiteral("exr")) {
    cv::Mat mat = impl_->mat_;
    if (mat.empty() || mat.type() != CV_32FC4) {
      return false;
    }
    if (!mat.isContinuous()) {
      mat = mat.clone();
    }

    std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(path.toUtf8().constData());
    if (!out) {
      return false;
    }

    OIIO::ImageSpec spec(mat.cols, mat.rows, 4, OIIO::TypeDesc::FLOAT);
    spec.channelnames = {"R", "G", "B", "A"};
    if (!out->open(path.toUtf8().constData(), spec)) {
      return false;
    }
    if (!out->write_image(OIIO::TypeDesc::FLOAT, mat.ptr<float>())) {
      out->close();
      return false;
    }
    return out->close();
  }

  return toQImage().save(path);
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

  const float* ImageF32x4_RGBA::rgba32fData() const
  {
   if (impl_->mat_.empty() || impl_->mat_.type() != CV_32FC4 || !impl_->mat_.isContinuous()) {
    return nullptr;
   }
   return impl_->mat_.ptr<float>();
  }

  float* ImageF32x4_RGBA::rgba32fData()
  {
   if (impl_->mat_.empty() || impl_->mat_.type() != CV_32FC4 || !impl_->mat_.isContinuous()) {
    return nullptr;
   }
   return impl_->mat_.ptr<float>();
  }

  const std::uint8_t* ImageF32x4_RGBA::rgba8Data() const
  {
   if (impl_->mat_.empty() || impl_->mat_.type() != CV_8UC4 || !impl_->mat_.isContinuous()) {
    return nullptr;
   }
   return impl_->mat_.ptr<std::uint8_t>();
  }

  std::uint8_t* ImageF32x4_RGBA::rgba8Data()
  {
   if (impl_->mat_.empty() || impl_->mat_.type() != CV_8UC4 || !impl_->mat_.isContinuous()) {
    return nullptr;
   }
   return impl_->mat_.ptr<std::uint8_t>();
  }

  QImage ImageF32x4_RGBA::toQImage() const
  {
   return CvUtils::cvMatToQImage(impl_->mat_);
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

  void ImageF32x4_RGBA::setFromCVMat(const cv::Mat& mat)
  {
    if (mat.empty()) return;

    cv::Mat tmp;
    // Convert various types to CV_32FC4 (RGBA float)
    if (mat.type() == CV_8UC1) {
      cv::cvtColor(mat, tmp, cv::COLOR_GRAY2RGBA);
      tmp.convertTo(tmp, CV_32F, 1.0/255.0);
    } else if (mat.type() == CV_8UC3) {
      cv::cvtColor(mat, tmp, cv::COLOR_BGR2RGBA);
      tmp.convertTo(tmp, CV_32F, 1.0/255.0);
    } else if (mat.type() == CV_8UC4) {
      mat.convertTo(tmp, CV_32F, 1.0/255.0);
    } else if (mat.type() == CV_16UC1) {
      cv::Mat f;
      mat.convertTo(f, CV_32F, 1.0/65535.0);
      cv::cvtColor(f, tmp, cv::COLOR_GRAY2RGBA);
    } else if (mat.type() == CV_16UC3) {
      cv::Mat f;
      mat.convertTo(f, CV_32F, 1.0/65535.0);
      cv::cvtColor(f, tmp, cv::COLOR_BGR2RGBA);
    } else if (mat.type() == CV_32FC3) {
      cv::cvtColor(mat, tmp, cv::COLOR_BGR2RGBA);
      tmp.convertTo(tmp, CV_32F);
    } else if (mat.type() == CV_32FC4) {
      // zero-copy: directly take ownership of the provided float RGBA mat
      impl_->mat_ = mat; // shares the underlying buffer (refcounted)
      return;
    } else {
      // Fallback: convert to RGBA 8-bit then to float
      cv::Mat bgr8;
      mat.convertTo(bgr8, CV_8U, 255.0);
      cv::cvtColor(bgr8, tmp, cv::COLOR_BGR2RGBA);
      tmp.convertTo(tmp, CV_32F, 1.0/255.0);
    }

    // Ensure tmp is CV_32FC4
    if (tmp.type() != CV_32FC4) {
      cv::Mat conv;
      tmp.convertTo(conv, CV_32F);
      if (conv.channels() == 1) cv::cvtColor(conv, conv, cv::COLOR_GRAY2RGBA);
      else if (conv.channels() == 3) cv::cvtColor(conv, conv, cv::COLOR_BGR2RGBA);
      impl_->mat_ = conv; // move converted mat
    } else {
      impl_->mat_ = tmp; // assign without extra clone
    }
  }

  void ImageF32x4_RGBA::setFromRGBA32F(const float* data, int width, int height)
  {
   if (!data || width <= 0 || height <= 0) {
    impl_->mat_.release();
    return;
   }
   cv::Mat mat(height, width, CV_32FC4, const_cast<float*>(data));
   setFromCVMat(mat);
  }

  void ImageF32x4_RGBA::setFromRGBA8(const std::uint8_t* data, int width, int height)
  {
   if (!data || width <= 0 || height <= 0) {
    impl_->mat_.release();
    return;
   }
   cv::Mat mat(height, width, CV_8UC4, const_cast<std::uint8_t*>(data));
   setFromCVMat(mat);
  }

  ImageF32x4_RGBA ImageF32x4_RGBA::DeepCopy() const
  {
   ImageF32x4_RGBA copy;
   copy.impl_->mat_ = impl_->mat_.clone();
   return copy;
  }

  // ピクセルアクセス
  FloatRGBA ImageF32x4_RGBA::getPixel(int x, int y) const
  {
   return impl_->getPixel(x, y);
  }

  void ImageF32x4_RGBA::setPixel(int x, int y, const FloatRGBA& color)
  {
   impl_->setPixel(x, y, color);
  }

  // 画像変換
  void ImageF32x4_RGBA::flipHorizontal()
  {
   cv::flip(impl_->mat_, impl_->mat_, 1);
  }

  void ImageF32x4_RGBA::flipVertical()
  {
   cv::flip(impl_->mat_, impl_->mat_, 0);
  }

  ImageF32x4_RGBA ImageF32x4_RGBA::crop(int x, int y, int cropWidth, int cropHeight) const
  {
   ImageF32x4_RGBA result;
   
   // 範囲チェック
   if (x < 0 || y < 0 || x + cropWidth > width() || y + cropHeight > height()) {
    return result; // 空の画像を返す
   }
   
   cv::Rect roi(x, y, cropWidth, cropHeight);
   result.impl_->mat_ = impl_->mat_(roi).clone();
   
   return result;
  }

  // ブレンディング
  void ImageF32x4_RGBA::alphaBlend(const ImageF32x4_RGBA& overlay, float opacity)
  {
   if (impl_->mat_.size() != overlay.impl_->mat_.size()) {
    return; // サイズが異なる場合は何もしない
   }
   
   cv::Mat result;
   
   // アルファチャンネルを取得
   std::vector<cv::Mat> bgChannels, fgChannels;
   cv::split(impl_->mat_, bgChannels);
   cv::split(overlay.impl_->mat_, fgChannels);
   
   if (bgChannels.size() < 4 || fgChannels.size() < 4) {
    return;
   }
   
   // アルファブレンディングの計算
   cv::Mat alpha = fgChannels[3] * opacity;
   cv::Mat invAlpha = 1.0f - alpha;
   
   std::vector<cv::Mat> outChannels(4);
   for (int i = 0; i < 3; ++i) {
    outChannels[i] = fgChannels[i].mul(alpha) + bgChannels[i].mul(invAlpha);
   }
   
   // アルファチャンネルの合成（MatExprを明示的にMatに変換）
   cv::Mat alphaBlended;
   cv::Mat temp1 = fgChannels[3] * opacity;
   cv::Mat temp2 = bgChannels[3].mul(fgChannels[3]) * opacity;
   alphaBlended = bgChannels[3] + temp1 - temp2;
   outChannels[3] = alphaBlended;
   
   cv::merge(outChannels, impl_->mat_);
  }

  ImageF32x4_RGBA ImageF32x4_RGBA::blend(const ImageF32x4_RGBA& other, float weight) const
  {
   ImageF32x4_RGBA result;
   
   if (impl_->mat_.size() != other.impl_->mat_.size()) {
    return *this; // サイズが異なる場合は自分を返す
   }
   
   cv::addWeighted(impl_->mat_, 1.0f - weight, other.impl_->mat_, weight, 0.0, result.impl_->mat_);
   
   return result;
  }

};


