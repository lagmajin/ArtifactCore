module;
#include <algorithm>
#include <utility>

module Graphics.Texture;

namespace ArtifactCore {

 class GPUTexture::Impl {
 public:
  int width_ = 0;
  int height_ = 0;
  GPUTextureFormat format_ = GPUTextureFormat::Unknown;
  int mipLevels_ = 0;
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

 bool GPUTexture::Create(const int width, const int height,
                         const GPUTextureFormat format, const int mipLevels)
 {
  if (!impl_) impl_ = new Impl();
  if (width <= 0 || height <= 0 || mipLevels <= 0 ||
      format == GPUTextureFormat::Unknown) {
   Reset();
   return false;
  }
  int maximumMipLevels = 1;
  for (int dimension = std::max(width, height); dimension > 1; dimension >>= 1)
   ++maximumMipLevels;
  if (mipLevels > maximumMipLevels) {
   Reset();
   return false;
  }
  impl_->width_ = width;
  impl_->height_ = height;
  impl_->format_ = format;
  impl_->mipLevels_ = mipLevels;
  return true;
 }

 void GPUTexture::Reset()
 {
  if (!impl_) return;
  impl_->width_ = 0;
  impl_->height_ = 0;
  impl_->format_ = GPUTextureFormat::Unknown;
  impl_->mipLevels_ = 0;
 }

 bool GPUTexture::IsValid() const
 {
  return impl_ && impl_->width_ > 0 && impl_->height_ > 0 &&
         impl_->mipLevels_ > 0 && impl_->format_ != GPUTextureFormat::Unknown;
 }

 int GPUTexture::GetWidth() const
 {
  return impl_ ? impl_->width_ : 0;
 }

 int GPUTexture::GetHeight() const
 {
  return impl_ ? impl_->height_ : 0;
 }

 GPUTextureFormat GPUTexture::GetFormat() const
 {
  return impl_ ? impl_->format_ : GPUTextureFormat::Unknown;
 }

 int GPUTexture::GetMipLevels() const
 {
  return impl_ ? impl_->mipLevels_ : 0;
 }

};
