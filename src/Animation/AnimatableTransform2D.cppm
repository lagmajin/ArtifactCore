module;


module Animation.Transform2D;


namespace ArtifactCore {

 class AnimatableTransform2D::Impl
 {
 private:

 public:
    Impl();
	~Impl();
	AnimatableValueT<float> posX_;
	AnimatableValueT<float> posY_;

	// #tag rotation
	AnimatableValueT<float> rotation_;

	// #tag scale
	AnimatableValueT<float> scaleX_;
	AnimatableValueT<float> scaleY_;
 };

 AnimatableTransform2D::Impl::Impl()
 {

 }

 AnimatableTransform2D::Impl::~Impl()
 {

 }
 void AnimatableTransform2D::setRotation(float degrees)
 {
  
 }

 void AnimatableTransform2D::setPosition(float x, float y)
 {
  //posX_.set(x);
  //posY_.set(y);
 }

 void AnimatableTransform2D::setScale(float sx, float sy)
 {
 
 }







};