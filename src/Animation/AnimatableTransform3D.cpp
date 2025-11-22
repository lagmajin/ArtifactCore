module;

module Animation.Transform3D;


namespace ArtifactCore
{
 class AnimatableTransform3D::Impl
 {
 private:

  public:
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

};