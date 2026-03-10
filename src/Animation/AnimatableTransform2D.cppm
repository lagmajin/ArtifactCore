module;

module Animation.Transform2D;

namespace ArtifactCore {

 class AnimatableTransform2D::Impl
 {
 public:
  AnimatableValueT<float> posX_;
  AnimatableValueT<float> posY_;
  AnimatableValueT<float> rotation_;
  AnimatableValueT<float> scaleX_;
  AnimatableValueT<float> scaleY_;

  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
 };

 AnimatableTransform2D::AnimatableTransform2D() : impl_(new Impl()) {}
 AnimatableTransform2D::~AnimatableTransform2D() { delete impl_; }

 AnimatableTransform2D::AnimatableTransform2D(const AnimatableTransform2D& other) : impl_(new Impl(*other.impl_)) {}
 AnimatableTransform2D& AnimatableTransform2D::operator=(const AnimatableTransform2D& other) {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 AnimatableTransform2D::AnimatableTransform2D(AnimatableTransform2D&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
 }
 AnimatableTransform2D& AnimatableTransform2D::operator=(AnimatableTransform2D&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 void AnimatableTransform2D::setRotation(float degrees)
 {
  impl_->rotation_.setCurrent(degrees);
 }

 void AnimatableTransform2D::setPosition(float x, float y)
 {
  impl_->posX_.setCurrent(x);
  impl_->posY_.setCurrent(y);
 }

 void AnimatableTransform2D::setScale(float sx, float sy)
 {
  impl_->scaleX_.setCurrent(sx);
  impl_->scaleY_.setCurrent(sy);
 }

 size_t AnimatableTransform2D::size() const
 {
  return static_cast<size_t>(Element::Count);
 }

};