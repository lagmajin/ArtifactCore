module;

module Animation.Transform3D;
import std;
import Animation.Value;
//import Graphics.CBuffer.Constants;

namespace ArtifactCore
{
class AnimatableTransform3D::Impl
{
private:
   
public:
  bool isZVisible = false;
  float initial_x = 0;
  float initial_y = 0;
  
  AnimatableValueT<float> x_;
  AnimatableValueT<float> y_;
  AnimatableValueT<float> z_;
  AnimatableValueT<float> anchorX_;
  AnimatableValueT<float> anchorY_;
  AnimatableValueT<float> anchorZ_;
  float positionX_ = 0.0f;
  float positionY_ = 0.0f;
  float rotation_ = 0.0f;
  float scaleX_ = 1.0f;
  float scaleY_ = 1.0f;
  Impl();
  ~Impl();

};

 AnimatableTransform3D::Impl::Impl()
 {

 }

 AnimatableTransform3D::Impl::~Impl()
 {

 }

 AnimatableTransform3D::AnimatableTransform3D() :impl_(new Impl())
 {

 }

 AnimatableTransform3D::~AnimatableTransform3D()
 {
  delete impl_;
 }

void AnimatableTransform3D::setInitalAngle(const RationalTime& time, float angle/*=0*/)
{
  impl_->rotation_ = angle;
}

void AnimatableTransform3D::setInitialScale(const RationalTime& time, float xs, float ys)
{
  impl_->scaleX_ = xs;
  impl_->scaleY_ = ys;
}

void AnimatableTransform3D::setInitialPosition(const RationalTime& time, float px, float py)
{
  impl_->positionX_ = px;
  impl_->positionY_ = py;
}

void AnimatableTransform3D::setPosition(const RationalTime& time, float x, float y)
{
  impl_->positionX_ = x;
  impl_->positionY_ = y;
}

void AnimatableTransform3D::setRotation(const RationalTime& time, float degrees)
{
  impl_->rotation_ = degrees;
}

float AnimatableTransform3D::positionX() const
{
  return impl_->positionX_;
}

float AnimatableTransform3D::positionY() const
{
  return impl_->positionY_;
}

float AnimatableTransform3D::rotation() const
{
  return impl_->rotation_;
}

float AnimatableTransform3D::scaleX() const
{
  return impl_->scaleX_;
}

float AnimatableTransform3D::scaleY() const
{
  return impl_->scaleY_;
}

};