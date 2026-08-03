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
  // NOTE: The class name describes the logical image contract. The backing
  // CV_32FC4/CV_8UC4 memory follows colorDescriptor().channelOrder and may
  // therefore be BGRA. Callers crossing a GPU RGBA texture boundary must
  // normalize BGRA -> RGBA explicitly; these accessors do not reorder data.
  auto toCVMat() const -> cv::Mat;
  // Returns a continuous CV_32FC4 copy in fixed logical R,G,B,A order.
  // Unlike toCVMat(), this normalizes BGRA backing storage when necessary.
  auto toCanonicalRGBA32FC4() const -> cv::Mat;
  // Returns a continuous CV_32FC4 copy in fixed OpenCV B,G,R,A order.
  // This is the migration target for CPU/OpenCV-oriented processing.
  auto toCanonicalBGRA32FC4() const -> cv::Mat;
  ImageSurfaceView surfaceView() const noexcept;
  QImage toQImage() const;
  // Returns a pointer to contiguous 4-float pixels in backing-memory order,
  // not guaranteed logical R,G,B,A order. Inspect colorDescriptor() first.
  const float* rgba32fData() const;
  float* rgba32fData();
  // Returns a pointer to contiguous 4-byte pixels in backing-memory order,
  // not guaranteed logical R,G,B,A order. Inspect colorDescriptor() first.
  const std::uint8_t* rgba8Data() const;
  std::uint8_t* rgba8Data();
  SurfaceColorDescriptor colorDescriptor() const noexcept;
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
  // Set from an existing OpenCV Mat (various types supported). CV_32FC4 is
  // copied as-is when passed directly; provide a descriptor when the Mat is
  // BGRA or otherwise differs from canonical RGBA.
  void setFromCVMat(const cv::Mat& mat);
  void setFromCVMat(const cv::Mat& mat,
                    const SurfaceColorDescriptor& descriptor);
  // Despite the historical name, the no-descriptor overload assumes the
  // supplied memory is canonical RGBA. Use the descriptor overload for BGRA.
  void setFromRGBA32F(const float* data, int width, int height);
  void setFromRGBA32F(const float* data, int width, int height,
                      const SurfaceColorDescriptor& descriptor);
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
  ImageF32x4_RGBA(ImageF32x4_RGBA&&) noexcept = default;
  ImageF32x4_RGBA& operator=(ImageF32x4_RGBA&&) noexcept = default;

 private:
  class Impl;
  Impl* impl_;
 };

}
