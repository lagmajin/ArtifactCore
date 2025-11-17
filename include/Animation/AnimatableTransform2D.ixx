module ;
export module Animation.Transform2D;

import std;
import Animation.Value;

export namespace ArtifactCore {

 class AnimatableTransform2D {
 private:
  AnimatableValueT<float> posX_;
  AnimatableValueT<float> posY_;

  // #tag rotation
  AnimatableValueT<float> rotation_;

  // #tag scale
  AnimatableValueT<float> scaleX_;
  AnimatableValueT<float> scaleY_;

 public:
  void setPosition(float x, float y) {
   //posX_.set(x);
   //posY_.set(y);
  }

  // Rotation
  void setRotation(float degrees) {
   //rotation_.set(degrees);
  }

  // Scale
  void setScale(float sx, float sy) {
   //scaleX_.set(sx);
   //scaleY_.set(sy);
  }
 };




};