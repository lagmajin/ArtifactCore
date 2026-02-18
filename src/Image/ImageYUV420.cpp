module;
#include <QImage>
#include <opencv2/opencv.hpp>
module Image.ImageYUV420;

import std;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
//import <opencv2/opencv.hpp>;

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
  impl_->width_ = image.width();
  impl_->height_ = image.height();
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
  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; ++xx) {
      QRgb px = image.pixel(xx, yy);
      int r = qRed(px);
      int g = qGreen(px);
      int b = qBlue(px);

      // BT.601 luma
      int Y = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
      impl_->y_plane_[yy * w + xx] = clampByte(Y);
    }
  }

  // U/V: average 2x2 blocks
  for (int by = 0; by < uh; ++by) {
    for (int bx = 0; bx < uw; ++bx) {
      int sumU = 0;
      int sumV = 0;
      int count = 0;
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
          int sx = bx * 2 + dx;
          int sy = by * 2 + dy;
          if (sx >= w || sy >= h) continue;
          QRgb px = image.pixel(sx, sy);
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
  }
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

    for (int y=0;y<h;++y) {
        for (int x=0;x<w;++x) {
            FloatRGBA p = rgba.getPixel(x,y);
            int r = static_cast<int>(p.r() * 255.0f + 0.5f);
            int g = static_cast<int>(p.g() * 255.0f + 0.5f);
            int b = static_cast<int>(p.b() * 255.0f + 0.5f);
            int Y = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
            out.impl_->y_plane_[y * w + x] = static_cast<uint8_t>(std::clamp(Y,0,255));
        }
    }

    for (int by=0; by<uh; ++by) {
        for (int bx=0; bx<uw; ++bx) {
            int sumU = 0, sumV = 0, cnt = 0;
            for (int dy=0; dy<2; ++dy) for (int dx=0; dx<2; ++dx) {
                int sx = bx*2 + dx; int sy = by*2 + dy;
                if (sx >= w || sy >= h) continue;
                FloatRGBA p = rgba.getPixel(sx, sy);
                int r = static_cast<int>(p.r() * 255.0f + 0.5f);
                int g = static_cast<int>(p.g() * 255.0f + 0.5f);
                int b = static_cast<int>(p.b() * 255.0f + 0.5f);
                int U = static_cast<int>(-0.169 * r - 0.331 * g + 0.5 * b + 128.5);
                int V = static_cast<int>(0.5 * r - 0.419 * g - 0.081 * b + 128.5);
                sumU += U; sumV += V; ++cnt;
            }
            if (cnt==0) cnt=1;
            out.impl_->u_plane_[by*uw + bx] = static_cast<uint8_t>(std::clamp(sumU/cnt,0,255));
            out.impl_->v_plane_[by*uw + bx] = static_cast<uint8_t>(std::clamp(sumV/cnt,0,255));
        }
    }
    return out;
}

ImageF32x4_RGBA ImageYUV420::toImage32xRGBA() const {
    ImageF32x4_RGBA out;
    int w = impl_->width_;
    int h = impl_->height_;
    out.resize(w,h);
    int uw = (w + 1) / 2;
    int uh = (h + 1) / 2;
    for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
        int Y = impl_->y_plane_[y*w + x];
        int bx = x/2; int by = y/2;
        int U = impl_->u_plane_[by*uw + bx];
        int V = impl_->v_plane_[by*uw + bx];
        int C = Y;
        int R = static_cast<int>(C + 1.402 * (V - 128));
        int G = static_cast<int>(C - 0.344136 * (U - 128) - 0.714136 * (V - 128));
        int B = static_cast<int>(C + 1.772 * (U - 128));
        auto clampf = [](int v)->float { if (v<0) v=0; if (v>255) v=255; return v/255.0f; };
        out.setPixel(x,y, FloatRGBA(clampf(R), clampf(G), clampf(B), 1.0f));
    }
    return out;
}

ImageYUV420 ImageYUV420::fromPlanes(const cv::Mat& yPlane, const cv::Mat& uPlane, const cv::Mat& vPlane) {
    ImageYUV420 out;
    int h = yPlane.rows; int w = yPlane.cols;
    out.impl_->width_ = w; out.impl_->height_ = h;
    out.impl_->y_plane_.assign(w*h,0);
    for (int y=0;y<h;++y) for (int x=0;x<w;++x) out.impl_->y_plane_[y*w + x] = static_cast<uint8_t>(yPlane.at<float>(y,x) * 255.0f);
    int uw = uPlane.cols; int uh = uPlane.rows;
    out.impl_->u_plane_.assign(uw*uh,128);
    out.impl_->v_plane_.assign(uw*uh,128);
    for (int y=0;y<uh;++y) for (int x=0;x<uw;++x) {
        out.impl_->u_plane_[y*uw + x] = static_cast<uint8_t>(uPlane.at<float>(y,x) * 255.0f);
        out.impl_->v_plane_[y*uw + x] = static_cast<uint8_t>(vPlane.at<float>(y,x) * 255.0f);
    }
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