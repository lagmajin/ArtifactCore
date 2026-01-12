module;

module Animation.Transform3D;
import std;
import Animation.Value;
import Graphics.CBuffer.Constants;

namespace ArtifactCore
{
 class AnimatableTransform3D::Impl
 {
 private:
 	
 public:
  bool isZVisible = false;
  float initial_x=0;
  float initial_y = 0;
 	
  AnimatableValueT<float> x_;
  AnimatableValueT<float> y_;
  AnimatableValueT<float> z_;
  AnimatableValueT<float> anchorX_;
  AnimatableValueT<float> anchorY_;
  AnimatableValueT<float> anchorZ_;
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

 }

 void AnimatableTransform3D::setInitialScale(const RationalTime& time, float xs, float ys)
 {

 }

 void AnimatableTransform3D::setInitialPosition(const RationalTime& time, float px, float py)
 {

 }

};