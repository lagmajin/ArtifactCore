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
#include <cstdint>
#include <limits>
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
import Graphics.SurfaceColorContract;
import Image.SurfacePixelConversion;

class tst_QList;

namespace ArtifactCore {

 class  ImageF32x4_RGBA::Impl
 {

 public:
  cv::Mat mat_;
  SurfaceColorDescriptor colorDescriptor_ = SurfaceColorDescriptor::unknownBgra32Float();
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
  
  const cv::Vec4f pixel = mat_.ptr<cv::Vec4f>(y)[x];
  return FloatRGBA(pixel[2], pixel[1], pixel[0], pixel[3]);
 }

 void ImageF32x4_RGBA::Impl::setPixel(int x, int y, const FloatRGBA& color)
 {
  if (x >= 0 && x < mat_.cols && y >= 0 && y < mat_.rows) {
   mat_.ptr<cv::Vec4f>(y)[x] =
       cv::Vec4f(color.b(), color.g(), color.r(), color.a());
  }
 }

 ImageF32x4_RGBA::Impl::Impl(const FloatRGBA& rgba, int width, int height)
  : mat_(height, width, CV_32FC4),
    colorDescriptor_(SurfaceColorDescriptor::unknownBgra32Float())
 {
  cv::Vec4f color(rgba.b(), rgba.g(), rgba.r(), rgba.a());
  mat_.setTo(color);
 }

 ImageF32x4_RGBA::Impl::Impl()
  : mat_(1, 1, CV_32FC4, cv::Scalar(0, 0, 0, 1)) // Default 1x1 with transparent
 {

 }

 ImageF32x4_RGBA::Impl::Impl(const Impl& other)
  : mat_(other.mat_.clone()), colorDescriptor_(other.colorDescriptor_)
 {

 }

 ImageF32x4_RGBA::Impl& ImageF32x4_RGBA::Impl::operator=(const Impl& other)
 {
  if (this != &other) {
    mat_ = other.mat_.clone();
    colorDescriptor_ = other.colorDescriptor_;
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

 ImageF32x4_RGBA::ImageF32x4_RGBA(ImageF32x4_RGBA&& other) noexcept
  : impl_(other.impl_)
 {
  other.impl_ = new Impl();
 }

 ImageF32x4_RGBA& ImageF32x4_RGBA::operator=(ImageF32x4_RGBA&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = new Impl();
  }
  return *this;
 }

  void ImageF32x4_RGBA::fill(const FloatRGBA& rgba)
  {
   cv::Vec4f color(rgba.b(), rgba.g(), rgba.r(), rgba.a());
   impl_->mat_.setTo(color);
   if (impl_->colorDescriptor_.channelOrder == SurfaceChannelOrder::Unknown) {
    impl_->colorDescriptor_ = SurfaceColorDescriptor::unknownBgra32Float();
   }
 }

 bool ImageF32x4_RGBA::load(const QString& path)
 {
  const QByteArray utf8Path = path.toUtf8();
  OIIO::ImageBuf source(utf8Path.constData());
  if (source.read(0, 0, true, OIIO::TypeDesc::FLOAT)) {
    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    if (spec.width > 0 && spec.height > 0 && spec.nchannels > 0) {
      OIIO::ImageBuf rgba;
      if (spec.nchannels >= 3) {
        std::array<int, 4> channelOrder{-1, -1, -1, -1};
        const char* canonicalNames[] = {"r", "g", "b", "a"};
        for (int channel = 0; channel < 4; ++channel) {
          for (int candidate = 0;
               candidate < spec.nchannels &&
               candidate < static_cast<int>(spec.channelnames.size());
               ++candidate) {
            const QString name = QString::fromStdString(
                spec.channelnames[static_cast<std::size_t>(candidate)]).toLower();
            if (name == QLatin1String(canonicalNames[channel]) ||
                (channel == 0 && name.endsWith(QLatin1String(".r"))) ||
                (channel == 1 && name.endsWith(QLatin1String(".g"))) ||
                (channel == 2 && name.endsWith(QLatin1String(".b"))) ||
                (channel == 3 && name.endsWith(QLatin1String(".a")))) {
              channelOrder[static_cast<std::size_t>(channel)] = candidate;
              break;
            }
          }
        }
        if (channelOrder[0] < 0 || channelOrder[1] < 0 || channelOrder[2] < 0) {
          channelOrder[0] = 0;
          channelOrder[1] = 1;
          channelOrder[2] = 2;
        }
        if (channelOrder[3] < 0 && spec.nchannels >= 4)
          channelOrder[3] = 3;
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(
            oriented, 4, channelOrder, channelValues);
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
        setFromCVMat(mat, SurfaceColorDescriptor::unknownRgba32Float());
        return true;
      }
    }
  }

  QImage img(path);
  if (img.isNull()) return false;
  const QImage encodedPremultiplied =
      img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  setFromCVMat(
      CvUtils::qImageToCvMat(encodedPremultiplied),
      SurfaceColorDescriptor::legacyOpenCvBgra32Float(
          TransferFunction::sRGB, SurfaceAlphaMode::Premultiplied));
  return true;
 }

 bool ImageF32x4_RGBA::save(const QString& path) const
 {
  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == QStringLiteral("exr")) {
    cv::Mat mat = toCanonicalRGBA32FC4();
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

  cv::Mat ImageF32x4_RGBA::toCanonicalRGBA32FC4() const
  {
   if (impl_->mat_.empty() || impl_->mat_.type() != CV_32FC4) {
    return {};
   }
   if (impl_->colorDescriptor_.channelOrder != SurfaceChannelOrder::BGRA)
    return {};
   cv::Mat rgba;
   cv::cvtColor(impl_->mat_, rgba, cv::COLOR_BGRA2RGBA);
   return rgba;
  }

  cv::Mat ImageF32x4_RGBA::toCanonicalBGRA32FC4() const
  {
   if (impl_->mat_.empty() || impl_->mat_.type() != CV_32FC4) {
    return {};
   }
   if (impl_->colorDescriptor_.channelOrder != SurfaceChannelOrder::BGRA)
    return {};
   return impl_->mat_.clone();
  }

  ImageSurfaceView ImageF32x4_RGBA::surfaceView() const noexcept
  {
   return {rgba32fData(), width(), height(),
           static_cast<std::size_t>(width()) * 4u * sizeof(float),
           SurfacePrecision::Float32, colorDescriptor()};
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

  SurfaceColorDescriptor ImageF32x4_RGBA::colorDescriptor() const noexcept
  {
   return impl_->colorDescriptor_;
  }

  void ImageF32x4_RGBA::setColorDescriptor(
      const SurfaceColorDescriptor& descriptor) noexcept
  {
   impl_->colorDescriptor_ = descriptor;
   impl_->colorDescriptor_.storage = SurfacePixelStorage::RGBA32Float;
   impl_->colorDescriptor_.channelOrder = SurfaceChannelOrder::BGRA;
  }

  QImage ImageF32x4_RGBA::toQImage() const
  {
   SurfacePixelBuffer converted = convertSurfacePixels(
       rgba32fData(), rgba8Data(), width(), height(), impl_->colorDescriptor_,
       SurfacePixelTarget::Rgba8SrgbStraight);
   if (!converted.isValid()) {
    return QImage();
   }
   // Ownership of the converted buffer moves to the QImage, removing the
   // second full-frame copy the previous image.copy() made.
   auto* owned = new SurfacePixelBuffer(std::move(converted));
   const QImage image(reinterpret_cast<uchar*>(owned->bytes.data()),
                      static_cast<int>(owned->width),
                      static_cast<int>(owned->height),
                      static_cast<qsizetype>(owned->rowStride),
                      QImage::Format_RGBA8888,
                      [](void* info) { delete static_cast<SurfacePixelBuffer*>(info); },
                      owned);
   return image;
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

    // The no-descriptor overload follows OpenCV's BGR/BGRA convention and
    // stores the result in the canonical CPU BGRA layout.
    impl_->colorDescriptor_ = SurfaceColorDescriptor::unknownBgra32Float();

    cv::Mat tmp;
    // Normalize OpenCV inputs to the canonical CV_32FC4 BGRA backing layout.
    if (mat.type() == CV_8UC1) {
      cv::cvtColor(mat, tmp, cv::COLOR_GRAY2BGRA);
      tmp.convertTo(tmp, CV_32F, 1.0/255.0);
    } else if (mat.type() == CV_8UC3) {
      cv::cvtColor(mat, tmp, cv::COLOR_BGR2BGRA);
      tmp.convertTo(tmp, CV_32F, 1.0/255.0);
    } else if (mat.type() == CV_8UC4) {
      mat.convertTo(tmp, CV_32F, 1.0/255.0);
    } else if (mat.type() == CV_16UC1) {
      cv::Mat f;
      mat.convertTo(f, CV_32F, 1.0/65535.0);
      cv::cvtColor(f, tmp, cv::COLOR_GRAY2BGRA);
    } else if (mat.type() == CV_16UC3) {
      cv::Mat f;
      mat.convertTo(f, CV_32F, 1.0/65535.0);
      cv::cvtColor(f, tmp, cv::COLOR_BGR2BGRA);
    } else if (mat.type() == CV_32FC3) {
      cv::cvtColor(mat, tmp, cv::COLOR_BGR2BGRA);
      tmp.convertTo(tmp, CV_32F);
    } else if (mat.type() == CV_32FC4) {
      // Copy here to avoid aliasing caller-owned temporary buffers.
      impl_->mat_ = mat.clone();
      return;
    } else {
      // Fallback: convert to BGR 8-bit, then normalize to BGRA float.
      cv::Mat bgr8;
      mat.convertTo(bgr8, CV_8U, 255.0);
      cv::cvtColor(bgr8, tmp, cv::COLOR_BGR2BGRA);
      tmp.convertTo(tmp, CV_32F, 1.0/255.0);
    }

    // Ensure tmp is CV_32FC4
    if (tmp.type() != CV_32FC4) {
      cv::Mat conv;
      tmp.convertTo(conv, CV_32F);
      if (conv.channels() == 1) cv::cvtColor(conv, conv, cv::COLOR_GRAY2BGRA);
      else if (conv.channels() == 3) cv::cvtColor(conv, conv, cv::COLOR_BGR2BGRA);
      impl_->mat_ = conv; // move converted mat
    } else {
      impl_->mat_ = tmp; // assign without extra clone
    }
  }

  void ImageF32x4_RGBA::setFromCVMat(
      const cv::Mat& mat, const SurfaceColorDescriptor& descriptor)
  {
    if (mat.empty()) return;
    cv::Mat normalized;
    const int channelCount = mat.channels();
    if (channelCount == 4) {
      if (descriptor.channelOrder == SurfaceChannelOrder::RGBA) {
        cv::cvtColor(mat, normalized, cv::COLOR_RGBA2BGRA);
      } else if (descriptor.channelOrder == SurfaceChannelOrder::BGRA) {
        normalized = mat;
      } else {
        impl_->mat_.release();
        impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
        return;
      }
    } else if (channelCount == 3) {
      if (descriptor.channelOrder == SurfaceChannelOrder::RGB) {
        cv::cvtColor(mat, normalized, cv::COLOR_RGB2BGR);
      } else if (descriptor.channelOrder == SurfaceChannelOrder::BGR) {
        normalized = mat;
      } else {
        impl_->mat_.release();
        impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
        return;
      }
    } else if (channelCount == 1 &&
               descriptor.channelOrder == SurfaceChannelOrder::Gray) {
      normalized = mat;
    } else {
      impl_->mat_.release();
      impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
      return;
    }
    setFromCVMat(normalized);
    impl_->colorDescriptor_ = descriptor;
    impl_->colorDescriptor_.storage = SurfacePixelStorage::RGBA32Float;
    impl_->colorDescriptor_.channelOrder = SurfaceChannelOrder::BGRA;
  }

  void ImageF32x4_RGBA::setFromCVMatOwned(
      cv::Mat&& mat, const SurfaceColorDescriptor& descriptor)
  {
    if (mat.empty() || mat.type() != CV_32FC4 ||
        descriptor.channelOrder != SurfaceChannelOrder::BGRA) {
      setFromCVMat(mat, descriptor);
      return;
    }
    impl_->mat_ = std::move(mat);
    impl_->colorDescriptor_ = descriptor;
    impl_->colorDescriptor_.storage = SurfacePixelStorage::RGBA32Float;
  }

  void ImageF32x4_RGBA::setFromRGBA32F(const float* data, int width, int height)
  {
   setFromRGBA32F(data, width, height,
                  SurfaceColorDescriptor::unknownRgba32Float());
  }

  void ImageF32x4_RGBA::setFromRGBA32F(
      const float* data, int width, int height,
      const SurfaceColorDescriptor& descriptor)
  {
   if (!data || width <= 0 || height <= 0) {
    impl_->mat_.release();
    impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
    return;
   }
   const cv::Mat source(height, width, CV_32FC4, const_cast<float*>(data));
   auto inputDescriptor = descriptor;
   if (inputDescriptor.channelOrder == SurfaceChannelOrder::Unknown)
     inputDescriptor.channelOrder = SurfaceChannelOrder::RGBA;
   setFromCVMat(source, inputDescriptor);
  }

  void ImageF32x4_RGBA::setFromBGRA32F(const float* data, int width, int height)
  {
   setFromBGRA32F(data, width, height,
                  SurfaceColorDescriptor::unknownBgra32Float());
  }

  void ImageF32x4_RGBA::setFromBGRA32F(
      const float* data, int width, int height,
      const SurfaceColorDescriptor& descriptor)
  {
   if (!data || width <= 0 || height <= 0) {
    impl_->mat_.release();
    impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
    return;
   }
   const cv::Mat source(height, width, CV_32FC4, const_cast<float*>(data));
   auto inputDescriptor = descriptor;
   inputDescriptor.storage = SurfacePixelStorage::RGBA32Float;
   inputDescriptor.channelOrder = SurfaceChannelOrder::BGRA;
   setFromCVMat(source, inputDescriptor);
  }

  bool ImageF32x4_RGBA::setFromGpuRGBA32F(
      const float* data, int width, int height, std::size_t rowStride,
      const SurfaceColorDescriptor& backingDescriptor)
  {
    if (!data || width <= 0 || height <= 0 ||
        backingDescriptor.storage != SurfacePixelStorage::RGBA32Float ||
        (backingDescriptor.channelOrder != SurfaceChannelOrder::RGBA &&
         backingDescriptor.channelOrder != SurfaceChannelOrder::BGRA)) {
      return false;
    }

    const auto rowBytes = static_cast<std::size_t>(width) * 4u * sizeof(float);
    if (rowStride < rowBytes || rowStride % alignof(float) != 0 ||
        reinterpret_cast<std::uintptr_t>(data) % alignof(float) != 0) {
      return false;
    }
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (static_cast<std::size_t>(height - 1) >
        (maxSize - rowBytes) / rowStride) {
      return false;
    }
    const auto span = static_cast<std::size_t>(height - 1) * rowStride + rowBytes;
    if (reinterpret_cast<std::uintptr_t>(data) >
        (std::numeric_limits<std::uintptr_t>::max)() - span) {
      return false;
    }

    const cv::Mat rgbaView(height, width, CV_32FC4,
                           const_cast<float*>(data), rowStride);
    auto inputDescriptor = backingDescriptor;
    inputDescriptor.storage = SurfacePixelStorage::RGBA32Float;
    inputDescriptor.channelOrder = SurfaceChannelOrder::RGBA;
    setFromCVMat(rgbaView, inputDescriptor);
    return !impl_->mat_.empty();
  }

  bool ImageF32x4_RGBA::setFromGpuBGRA32F(
      const float* data, int width, int height, std::size_t rowStride,
      const SurfaceColorDescriptor& backingDescriptor)
  {
    if (!data || width <= 0 || height <= 0 ||
        backingDescriptor.storage != SurfacePixelStorage::RGBA32Float ||
        (backingDescriptor.channelOrder != SurfaceChannelOrder::RGBA &&
         backingDescriptor.channelOrder != SurfaceChannelOrder::BGRA)) {
      return false;
    }

    const auto rowBytes = static_cast<std::size_t>(width) * 4u * sizeof(float);
    if (rowStride < rowBytes || rowStride % alignof(float) != 0 ||
        reinterpret_cast<std::uintptr_t>(data) % alignof(float) != 0) {
      return false;
    }
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (static_cast<std::size_t>(height - 1) >
        (maxSize - rowBytes) / rowStride) {
      return false;
    }
    const auto span = static_cast<std::size_t>(height - 1) * rowStride + rowBytes;
    if (reinterpret_cast<std::uintptr_t>(data) >
        (std::numeric_limits<std::uintptr_t>::max)() - span) {
      return false;
    }

    const cv::Mat bgraView(height, width, CV_32FC4,
                           const_cast<float*>(data), rowStride);
    auto inputDescriptor = backingDescriptor;
    inputDescriptor.storage = SurfacePixelStorage::RGBA32Float;
    inputDescriptor.channelOrder = SurfaceChannelOrder::BGRA;
    setFromCVMat(bgraView, inputDescriptor);
    return !impl_->mat_.empty();
  }

  void ImageF32x4_RGBA::setFromRGBA8(const std::uint8_t* data, int width, int height)
  {
   if (!data || width <= 0 || height <= 0) {
    impl_->mat_.release();
    impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
    return;
   }
   cv::Mat mat(height, width, CV_8UC4, const_cast<std::uint8_t*>(data));
   setFromCVMat(mat, SurfaceColorDescriptor::unknownRgba32Float());
  }

  void ImageF32x4_RGBA::setFromRGBA8(
      const std::uint8_t* data, int width, int height,
      const SurfaceColorDescriptor& descriptor)
  {
   if (!data || width <= 0 || height <= 0) {
    impl_->mat_.release();
    impl_->colorDescriptor_ = SurfaceColorDescriptor::unknown();
    return;
   }
   const cv::Mat source(height, width, CV_8UC4,
                        const_cast<std::uint8_t*>(data));
   auto inputDescriptor = descriptor;
   if (inputDescriptor.channelOrder == SurfaceChannelOrder::Unknown)
    inputDescriptor.channelOrder = SurfaceChannelOrder::RGBA;
   setFromCVMat(source, inputDescriptor);
  }

  ImageF32x4_RGBA ImageF32x4_RGBA::DeepCopy() const
  {
   ImageF32x4_RGBA copy;
   copy.impl_->mat_ = impl_->mat_.clone();
   copy.impl_->colorDescriptor_ = impl_->colorDescriptor_;
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
   result.impl_->colorDescriptor_ = impl_->colorDescriptor_;
   
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
   result.impl_->colorDescriptor_ =
       impl_->colorDescriptor_ == other.impl_->colorDescriptor_
           ? impl_->colorDescriptor_
           : SurfaceColorDescriptor::unknown();
   
   return result;
  }

};


