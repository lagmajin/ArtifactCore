module;
#include <QImage>
module Image.ImageYUV420;

import std;

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

  impl_->y_plane_.resize(w * h);
  impl_->u_plane_.resize((w / 2) * (h / 2));
  impl_->v_plane_.resize((w / 2) * (h / 2));
 }

 ImageYUV420::~ImageYUV420()
 {

 }

 ImageYUV420 ImageYUV420::fromQImage(const QImage& image)
 {

 	
  return QImage();
 }

};