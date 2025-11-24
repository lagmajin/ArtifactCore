module;

module Core.Scale.Zoom;


import std;


namespace ArtifactCore {

class ZoomScale2D::Impl
{
private:

public:
 Impl() = default;
 Impl(const Impl&) = default;
 Impl& operator=(const Impl&) = default;
 float scale_ = 1.0f;

 void ZoomIn(float factor);

 void ZoomOut(float factor);

 float scale() const;
};

void ZoomScale2D::Impl::ZoomIn(float factor)
{
 scale_ *= factor;
}

void ZoomScale2D::Impl::ZoomOut(float factor)
{
 scale_ /= factor;
}

float ZoomScale2D::Impl::scale() const
{
 return scale_;
}

 ZoomScale2D::ZoomScale2D():impl_(new Impl())
 {

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

 float ZoomScale2D::scale() const
 {
  return impl_->scale();
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