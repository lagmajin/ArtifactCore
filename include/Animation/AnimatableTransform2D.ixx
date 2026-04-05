module;
#include <cstddef>

export module Animation.Transform2D;

import Animation.Value;

export namespace ArtifactCore {

 class AnimatableTransform2D {
 private:
  class Impl;
  Impl* impl_;

 public:
  AnimatableTransform2D();
  ~AnimatableTransform2D();

  AnimatableTransform2D(const AnimatableTransform2D& other);
  AnimatableTransform2D& operator=(const AnimatableTransform2D& other);
  AnimatableTransform2D(AnimatableTransform2D&& other) noexcept;
  AnimatableTransform2D& operator=(AnimatableTransform2D&& other) noexcept;

  enum class Element {
   Position,
   Rotation,
   Scale,
   Count
  };

  void setPosition(float x, float y);
  void setRotation(float degrees);
  void setScale(float sx, float sy);

  void position(float& x, float& y) const;
  float rotation() const;
  void scale(float& sx, float& sy) const;

  size_t size() const;
 };

};
