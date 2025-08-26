module;

module Core.Scale2D;


import std;


namespace ArtifactCore {

class ZoomScale2D::Impl
{
private:

public:
 Impl();
 ~Impl();
 float scale_ = 1.0f;

 void ZoomIn(float factor);

 void ZoomOut(float factor);

 float scale() const;
};

ZoomScale2D::Impl::Impl()
{

}

ZoomScale2D::Impl::~Impl()
{

}

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

 ZoomScale2D::ZoomScale2D(const ZoomScale2D& scale) :impl_(new Impl())
 {

 }

 ZoomScale2D::~ZoomScale2D()
 {
  delete impl_;
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

};