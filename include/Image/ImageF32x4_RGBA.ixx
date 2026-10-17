module;
#include "../Define/DllExportMacro.hpp"

//#include <folly/concurrency/AtomicSharedPtr.h>


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
#include <cstddef>
#include <array>
#include <chrono>

//#include <type_traits>
//#include <variant>
#include <any>
//#include <atomic>
#include <queue>
#include <deque>
#include <QImage>
#include <QUuid>
#include <QObject>
#include <QString>
#include <opencv2/core/mat.hpp>
export module Image.ImageF32x4_RGBA;

import Size;
import FloatRGBA;
import ImageInterface;
import Graphics.SurfaceColorContract;
import Image.ImageSurfaceView;


export namespace ArtifactCore {



  class LIBRARY_DLL_API ImageF32x4_RGBA : public QObject, public ImageInterface {
 public:
  ImageF32x4_RGBA();
  explicit ImageF32x4_RGBA(const FloatRGBA& color);
  ImageF32x4_RGBA(const ImageF32x4_RGBA& image);
  ~ImageF32x4_RGBA();
  
  // 基本操作
  // NOTE: The class name describes the logical image API. Canonical CPU
  // CV_32FC4/CV_8UC4 backing is BGRA; raw accessors do not reorder data.
  auto toCVMat() const -> cv::Mat;
  // Returns a continuous CV_32FC4 copy in GPU/API R,G,B,A order.
  auto toCanonicalRGBA32FC4() const -> cv::Mat;
  // Returns a continuous CV_32FC4 copy in canonical CPU B,G,R,A order.
  auto toCanonicalBGRA32FC4() const -> cv::Mat;
  ImageSurfaceView surfaceView() const noexcept;
  QImage toQImage() const;
  // Returns a pointer to contiguous 4-float pixels in canonical B,G,R,A order.
  const float* rgba32fData() const;
  float* rgba32fData();
  // Returns a pointer to contiguous 4-byte pixels in canonical B,G,R,A order.
  const std::uint8_t* rgba8Data() const;
  std::uint8_t* rgba8Data();
  SurfaceColorDescriptor colorDescriptor() const noexcept;
  // Updates color/alpha metadata while preserving this image's Float32 BGRA
  // storage contract. External channel orders must be normalized at ingress.
  void setColorDescriptor(const SurfaceColorDescriptor& descriptor) noexcept;
  void fill(const FloatRGBA& rgba);
  bool load(const QString& path);
  bool save(const QString& path) const;
  void fillAlpha(float alpha=1.0f);
  void resize(int width,int height);

  // サイズ情報
  int width() const;
  int height() const;
  bool isEmpty() const { return width() <= 0 || height() <= 0; }
  size_t totalPixels() const { return static_cast<size_t>(width() * height()); }

  // ピクセルアクセス
  FloatRGBA getPixel(int x, int y) const;
  void setPixel(int x, int y, const FloatRGBA& color);

  // 画像変換
  void flipHorizontal();
  void flipVertical();
  ImageF32x4_RGBA crop(int x, int y, int width, int height) const;
  // Set from an OpenCV Mat (various types supported). The no-descriptor
  // overload follows OpenCV's BGR/BGRA convention. The descriptor overload
  // identifies the producer order; storage is normalized to BGRA.
  void setFromCVMat(const cv::Mat& mat);
  void setFromCVMat(const cv::Mat& mat,
                    const SurfaceColorDescriptor& descriptor);
  // Takes ownership of a caller-created CV_32FC4 buffer without cloning it.
  void setFromCVMatOwned(cv::Mat&& mat,
                         const SurfaceColorDescriptor& descriptor);
  // The no-descriptor overload accepts logical RGBA input and stores BGRA.
  // The descriptor overload identifies the input order; output storage stays BGRA.
  void setFromRGBA32F(const float* data, int width, int height);
  void setFromRGBA32F(const float* data, int width, int height,
                      const SurfaceColorDescriptor& descriptor);
  void setFromBGRA32F(const float* data, int width, int height);
  void setFromBGRA32F(const float* data, int width, int height,
                      const SurfaceColorDescriptor& descriptor);
  // Copies a mapped GPU RGBA32F image into canonical CPU BGRA backing.
  bool setFromGpuRGBA32F(const float* data, int width, int height,
                         std::size_t rowStride,
                         const SurfaceColorDescriptor& backingDescriptor);
  // Copies a mapped BGRA-native Artifact GPU image into canonical CPU BGRA backing.
  bool setFromGpuBGRA32F(const float* data, int width, int height,
                         std::size_t rowStride,
                         const SurfaceColorDescriptor& backingDescriptor);
  void setFromRGBA8(const std::uint8_t* data, int width, int height);
  void setFromRGBA8(const std::uint8_t* data, int width, int height,
                    const SurfaceColorDescriptor& descriptor);
  
  // ブレンディング
  void alphaBlend(const ImageF32x4_RGBA& overlay, float opacity = 1.0f);
  ImageF32x4_RGBA blend(const ImageF32x4_RGBA& other, float weight) const;

  // コピー操作
  ImageF32x4_RGBA DeepCopy() const;
  ImageF32x4_RGBA createMaskLike(const ImageF32x4_RGBA& src, const FloatRGBA& fillColor);

  // 演算子
  ImageF32x4_RGBA& operator=(const ImageF32x4_RGBA& other);
  // Move transfers impl_ ownership; the moved-from image keeps a fresh
  // empty Impl so it stays usable (a defaulted move would shallow-copy
  // impl_ and double-delete).
  ImageF32x4_RGBA(ImageF32x4_RGBA&& other) noexcept;
  ImageF32x4_RGBA& operator=(ImageF32x4_RGBA&& other) noexcept;

 private:
  class Impl;
  Impl* impl_;
 };

}
