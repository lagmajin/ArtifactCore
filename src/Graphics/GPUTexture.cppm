module;
#include <utility>

module Graphics.Texture;

namespace ArtifactCore {

 class GPUTexture::Impl {
 public:
  int width_ = 0;
  int height_ = 0;
  Impl() = default;
 };

 GPUTexture::GPUTexture() : impl_(new Impl())
 {
 }

  GPUTexture::~GPUTexture()
  {
   delete impl_;
  }

  GPUTexture::GPUTexture(GPUTexture&& other) noexcept : impl_(other.impl_)
  {
   other.impl_ = nullptr;
  }

  GPUTexture& GPUTexture::operator=(GPUTexture&& other) noexcept
  {
   if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
   }
   return *this;
  }

 int GPUTexture::GetWidth() const
 {
  return impl_->width_;
 }

 int GPUTexture::GetHeight() const
 {
  return impl_->height_;
 }

};
