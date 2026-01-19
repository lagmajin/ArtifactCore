module;

module Core.Scale.Zoom;


import std;


namespace ArtifactCore {

class ZoomScale2D::Impl
{
public:
 float minScale_ = 0.1f;
 float maxScale_ = 10.0f;
 float scale_ = 1.0f;

 void ZoomIn(float factor);
 void ZoomOut(float factor);
 void setScale(float scale);
 float scale() const;
 void setMinScale(float minScale);
 void setMaxScale(float maxScale);
 float minScale() const;
 float maxScale() const;
 void reset();
 bool isDefault() const;
 std::string toString() const;
};

void ZoomScale2D::Impl::ZoomIn(float factor)
{
 scale_ *= factor;
 scale_ = std::clamp(scale_, minScale_, maxScale_);
}

void ZoomScale2D::Impl::ZoomOut(float factor)
{
 scale_ /= factor;
 scale_ = std::clamp(scale_, minScale_, maxScale_);
}

void ZoomScale2D::Impl::setScale(float scale)
{
 scale_ = std::clamp(scale, minScale_, maxScale_);
}

float ZoomScale2D::Impl::scale() const
{
 return scale_;
}

void ZoomScale2D::Impl::setMinScale(float minScale)
{
 minScale_ = minScale;
 if (scale_ < minScale_) scale_ = minScale_;
}

void ZoomScale2D::Impl::setMaxScale(float maxScale)
{
 maxScale_ = maxScale;
 if (scale_ > maxScale_) scale_ = maxScale_;
}

float ZoomScale2D::Impl::minScale() const
{
 return minScale_;
}

float ZoomScale2D::Impl::maxScale() const
{
 return maxScale_;
}

void ZoomScale2D::Impl::reset()
{
 scale_ = 1.0f;
}

bool ZoomScale2D::Impl::isDefault() const
{
 return scale_ == 1.0f;
}

std::string ZoomScale2D::Impl::toString() const
{
 return std::to_string(scale_);
}

 ZoomScale2D::ZoomScale2D():impl_(new Impl())
 {

 }

 ZoomScale2D::ZoomScale2D(float initialScale):impl_(new Impl())
 {
  impl_->setScale(initialScale);
 }

 ZoomScale2D::ZoomScale2D(const ZoomScale2D& other)
  : impl_(new Impl(*other.impl_)) // Implのコピーコンストラクタを利用
 {
 }

 ZoomScale2D::~ZoomScale2D()
 {
  delete impl_;
  impl_ = nullptr;
 }

 void ZoomScale2D::ZoomIn(float factor)
 {
  impl_->ZoomIn(factor);
 }

 void ZoomScale2D::ZoomOut(float factor)
 {
  impl_->ZoomOut(factor);
 }

 void ZoomScale2D::setScale(float scale)
 {
  impl_->setScale(scale);
 }

 float ZoomScale2D::scale() const
 {
  return impl_->scale();
 }

 void ZoomScale2D::setMinScale(float minScale)
 {
  impl_->setMinScale(minScale);
 }

 void ZoomScale2D::setMaxScale(float maxScale)
 {
  impl_->setMaxScale(maxScale);
 }

 float ZoomScale2D::minScale() const
 {
  return impl_->minScale();
 }

 float ZoomScale2D::maxScale() const
 {
  return impl_->maxScale();
 }

 void ZoomScale2D::reset()
 {
  impl_->reset();
 }

 bool ZoomScale2D::isDefault() const
 {
  return impl_->isDefault();
 }

 std::string ZoomScale2D::toString() const
 {
  return impl_->toString();
 }

 ZoomScale2D& ZoomScale2D::operator+=(float delta)
{
 impl_->scale_ += delta;
 impl_->scale_ = std::clamp(impl_->scale_, impl_->minScale_, impl_->maxScale_);
 return *this;
}

ZoomScale2D& ZoomScale2D::operator-=(float delta)
{
 impl_->scale_ -= delta;
 impl_->scale_ = std::clamp(impl_->scale_, impl_->minScale_, impl_->maxScale_);
 return *this;
}

ZoomScale2D& ZoomScale2D::operator*=(float factor)
{
 impl_->scale_ *= factor;
 impl_->scale_ = std::clamp(impl_->scale_, impl_->minScale_, impl_->maxScale_);
 return *this;
}

ZoomScale2D& ZoomScale2D::operator/=(float factor)
{
 impl_->scale_ /= factor;
 impl_->scale_ = std::clamp(impl_->scale_, impl_->minScale_, impl_->maxScale_);
 return *this;
}

ZoomScale2D ZoomScale2D::operator*(float factor) const
{
 ZoomScale2D result(*this);
 result *= factor;
 return result;
}

ZoomScale2D ZoomScale2D::operator/(float factor) const
{
 ZoomScale2D result(*this);
 result /= factor;
 return result;
}

bool ZoomScale2D::operator==(const ZoomScale2D& other) const
{
 return impl_->scale_ == other.impl_->scale_;
}

bool ZoomScale2D::operator!=(const ZoomScale2D& other) const
{
 return !(*this == other);
 }

 ZoomScale2D& ZoomScale2D::operator=(const ZoomScale2D& other)
 {
  if (this != &other)
   *impl_ = *other.impl_;
  return *this;
 }

ZoomScale2D& ZoomScale2D::operator=(ZoomScale2D&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }
};