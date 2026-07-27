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

module Image.ImageYUV420;

import Core.Parallel;
import Image.ImageF32x4_RGBA;
import FloatRGBA;


namespace ArtifactCore {

 class ImageYUV420::Impl{
 private:

 public:
  std::vector<uint8_t> y_plane_;
  std::vector<uint8_t> u_plane_;
  std::vector<uint8_t> v_plane_;
  int width_ = 0;
  int height_ = 0;
};

 ImageYUV420::ImageYUV420():impl_(new Impl())
 {

 }

 ImageYUV420::ImageYUV420(const QImage& image):impl_(new Impl())
 {
  const QImage source = (image.format() == QImage::Format_RGB32 ||
                         image.format() == QImage::Format_ARGB32)
      ? image
      : image.convertToFormat(QImage::Format_ARGB32);
  impl_->width_ = source.width();
  impl_->height_ = source.height();
  int w = impl_->width_;
  int h = impl_->height_;

  impl_->y_plane_.assign(w * h, 0);
  int uw = (w + 1) / 2;
  int uh = (h + 1) / 2;
  impl_->u_plane_.assign(uw * uh, 128);
  impl_->v_plane_.assign(uw * uh, 128);

  auto clampByte = [](int v) -> uint8_t {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
  };

  // Fill Y plane and subsampled U/V (4:2:0) by averaging 2x2 blocks
  Parallel::For(0, h, w * h, [&](int yy) {
    const auto* row = reinterpret_cast<const QRgb*>(source.constScanLine(yy));
    for (int xx = 0; xx < w; ++xx) {
      QRgb px = row[xx];
      int r = qRed(px);
      int g = qGreen(px);
      int b = qBlue(px);

      // BT.601 luma
      int Y = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
      impl_->y_plane_[yy * w + xx] = clampByte(Y);
    }
  });

  // U/V: average 2x2 blocks
  Parallel::For(0, uh, uw * uh, [&](int by) {
    for (int bx = 0; bx < uw; ++bx) {
      int sumU = 0;
      int sumV = 0;
      int count = 0;
      const auto* row0 = reinterpret_cast<const QRgb*>(source.constScanLine(by * 2));
      const auto* row1 = (by * 2 + 1 < h)
          ? reinterpret_cast<const QRgb*>(source.constScanLine(by * 2 + 1))
          : nullptr;
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
          int sx = bx * 2 + dx;
          int sy = by * 2 + dy;
          if (sx >= w || sy >= h) continue;
          const auto* row = dy == 0 ? row0 : row1;
          QRgb px = row[sx];
          int r = qRed(px);
          int g = qGreen(px);
          int b = qBlue(px);
          int U = static_cast<int>(-0.169 * r - 0.331 * g + 0.5 * b + 128.5);
          int V = static_cast<int>(0.5 * r - 0.419 * g - 0.081 * b + 128.5);
          sumU += U;
          sumV += V;
          ++count;
        }
      }
      if (count == 0) count = 1;
      int avgU = sumU / count;
      int avgV = sumV / count;
      impl_->u_plane_[by * uw + bx] = clampByte(avgU);
      impl_->v_plane_[by * uw + bx] = clampByte(avgV);
    }
  });
 }

 ImageYUV420::~ImageYUV420()
 {

 }

// Create from ImageF32x4_RGBA
ImageYUV420 ImageYUV420::fromImage32xRGBA(const ImageF32x4_RGBA& rgba) {
    ImageYUV420 out;
    int w = rgba.width();
    int h = rgba.height();
    out.impl_->width_ = w;
    out.impl_->height_ = h;
    out.impl_->y_plane_.assign(w * h, 0);
    int uw = (w + 1) / 2;
    int uh = (h + 1) / 2;
    out.impl_->u_plane_.assign(uw * uh, 128);
    out.impl_->v_plane_.assign(uw * uh, 128);
    const float* sourcePixels = rgba.rgba32fData();

    Parallel::For(0, h, w * h, [&](int y) {
        for (int x=0;x<w;++x) {
            const float* p = sourcePixels +
                (static_cast<size_t>(y) * w + x) * 4u;
            int r = static_cast<int>(p[0] * 255.0f + 0.5f);
            int g = static_cast<int>(p[1] * 255.0f + 0.5f);
            int b = static_cast<int>(p[2] * 255.0f + 0.5f);
            int Y = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
            out.impl_->y_plane_[y * w + x] = static_cast<uint8_t>(std::clamp(Y,0,255));
        }
    });

    Parallel::For(0, uh, uw * uh, [&](int by) {
        for (int bx=0; bx<uw; ++bx) {
            int sumU = 0, sumV = 0, cnt = 0;
            for (int dy=0; dy<2; ++dy) for (int dx=0; dx<2; ++dx) {
                int sx = bx*2 + dx; int sy = by*2 + dy;
                if (sx >= w || sy >= h) continue;
                const float* p = sourcePixels +
                    (static_cast<size_t>(sy) * w + sx) * 4u;
                int r = static_cast<int>(p[0] * 255.0f + 0.5f);
                int g = static_cast<int>(p[1] * 255.0f + 0.5f);
                int b = static_cast<int>(p[2] * 255.0f + 0.5f);
                int U = static_cast<int>(-0.169 * r - 0.331 * g + 0.5 * b + 128.5);
                int V = static_cast<int>(0.5 * r - 0.419 * g - 0.081 * b + 128.5);
                sumU += U; sumV += V; ++cnt;
            }
            if (cnt==0) cnt=1;
            out.impl_->u_plane_[by*uw + bx] = static_cast<uint8_t>(std::clamp(sumU/cnt,0,255));
            out.impl_->v_plane_[by*uw + bx] = static_cast<uint8_t>(std::clamp(sumV/cnt,0,255));
        }
    });
    return out;
}

ImageF32x4_RGBA ImageYUV420::toImage32xRGBA() const {
    ImageF32x4_RGBA out;
    int w = impl_->width_;
    int h = impl_->height_;
    out.resize(w,h);
    float* outPixels = out.rgba32fData();
    if (!outPixels) return out;
    int uw = (w + 1) / 2;
    int uh = (h + 1) / 2;
    Parallel::For(0, h, w * h, [&](int y) {
      float* outRow = outPixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 4u;
      for (int x=0;x<w;++x) {
        int Y = impl_->y_plane_[y*w + x];
        int bx = x/2; int by = y/2;
        int U = impl_->u_plane_[by*uw + bx];
        int V = impl_->v_plane_[by*uw + bx];
        int C = Y;
        int R = static_cast<int>(C + 1.402 * (V - 128));
        int G = static_cast<int>(C - 0.344136 * (U - 128) - 0.714136 * (V - 128));
        int B = static_cast<int>(C + 1.772 * (U - 128));
        auto clampf = [](int v)->float { if (v<0) v=0; if (v>255) v=255; return v/255.0f; };
        float* pixel = outRow + static_cast<std::size_t>(x) * 4u;
        pixel[0] = clampf(R);
        pixel[1] = clampf(G);
        pixel[2] = clampf(B);
        pixel[3] = 1.0f;
    }});
    return out;
}

ImageYUV420 ImageYUV420::fromPlanes(const cv::Mat& yPlane, const cv::Mat& uPlane, const cv::Mat& vPlane) {
    ImageYUV420 out;
    int h = yPlane.rows; int w = yPlane.cols;
    out.impl_->width_ = w; out.impl_->height_ = h;
    out.impl_->y_plane_.assign(w*h,0);
    Parallel::For(0, h, w * h, [&](int y) {
        const float* row = yPlane.ptr<float>(y);
        for (int x=0;x<w;++x) out.impl_->y_plane_[y*w + x] = static_cast<uint8_t>(row[x] * 255.0f);
    });
    int uw = uPlane.cols; int uh = uPlane.rows;
    out.impl_->u_plane_.assign(uw*uh,128);
    out.impl_->v_plane_.assign(uw*uh,128);
    Parallel::For(0, uh, uw * uh, [&](int y) {
        const float* uRow = uPlane.ptr<float>(y);
        const float* vRow = vPlane.ptr<float>(y);
        for (int x=0;x<uw;++x) {
        out.impl_->u_plane_[y*uw + x] = static_cast<uint8_t>(uRow[x] * 255.0f);
        out.impl_->v_plane_[y*uw + x] = static_cast<uint8_t>(vRow[x] * 255.0f);
    }});
    return out;
}

cv::Mat ImageYUV420::yPlane() const {
    return cv::Mat(impl_->height_, impl_->width_, CV_8UC1, const_cast<uint8_t*>(impl_->y_plane_.data())).clone();
}

cv::Mat ImageYUV420::uPlane() const {
    int uw = (impl_->width_ + 1)/2; int uh = (impl_->height_ + 1)/2;
    return cv::Mat(uh, uw, CV_8UC1, const_cast<uint8_t*>(impl_->u_plane_.data())).clone();
}

cv::Mat ImageYUV420::vPlane() const {
    int uw = (impl_->width_ + 1)/2; int uh = (impl_->height_ + 1)/2;
    return cv::Mat(uh, uw, CV_8UC1, const_cast<uint8_t*>(impl_->v_plane_.data())).clone();
}

};
