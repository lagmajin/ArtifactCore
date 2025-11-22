module ;
export module Animation.Transform2D;

import std;
import Animation.Value;

export namespace ArtifactCore {

 class AnimatableTransform2D {
 private:
  class Impl;
  Impl* impl_;

 public:
  void setPosition(float x, float y);

  // Rotation
  void setRotation(float degrees);

  // Scale
  void setScale(float sx, float sy);
 };




};