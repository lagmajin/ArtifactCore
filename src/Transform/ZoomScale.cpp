module;

module Core.Scale2D;





namespace ArtifactCore {

class ZoomScale2D::Impl
{
private:

public:
 Impl();
 ~Impl();
 float scale = 1.0f;

 void ZoomIn(float factor);

 void ZoomOut(float factor);

 float GetScale() const;
};

ZoomScale2D::Impl::Impl()
{

}

ZoomScale2D::Impl::~Impl()
{

}

void ZoomScale2D::Impl::ZoomIn(float factor)
{
 scale *= factor;
}

void ZoomScale2D::Impl::ZoomOut(float factor)
{
 scale /= factor;
}

float ZoomScale2D::Impl::GetScale() const
{
 return scale;
}

 ZoomScale2D::ZoomScale2D():impl_(new Impl())
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

 float ZoomScale2D::GetScale() const
 {
  return impl_->GetScale();
 }

};