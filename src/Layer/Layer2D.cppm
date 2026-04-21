module;
#include <algorithm>
#include <utility>
#include <memory>

module Layer2D;

import Image;
import Layer2D;
import Layer.Blend;
import Layer.Matte;

namespace ArtifactCore {

 struct Layer2D::Impl
 {
  ImageF32x4 image;
  Layer2DSetting settings;  // opacity, blendMode, matteMode
 };


 Layer2D::Layer2D() : impl_(std::make_unique<Impl>())
 {
  impl_->settings.opacity = 1.0f;
  impl_->settings.blendMode = BlendMode::Normal;
  impl_->settings.matteMode = MatteMode::None;
 }

 Layer2D::~Layer2D()
 {

 }

 StaticTransform2D Layer2D::transform2D() const
 {
  return StaticTransform2D();
 }
 
 ImageF32x4_RGBA Layer2D::transformedLayer()
 {
  return ImageF32x4_RGBA();
 }

 // Opacity 関連
 float Layer2D::opacity() const
 {
  return impl_->settings.opacity;
 }
 
 void Layer2D::setOpacity(float value)
 {
  impl_->settings.opacity = std::clamp(value, 0.0f, 1.0f);
 }
 
 // Blend/Matte 関連
 BlendMode Layer2D::blendMode() const
 {
  return impl_->settings.blendMode;
 }
 
 void Layer2D::setBlendMode(BlendMode mode)
 {
  impl_->settings.blendMode = mode;
 }
 
 MatteMode Layer2D::matteMode() const
 {
  return impl_->settings.matteMode;
 }
 
  void Layer2D::setMatteMode(MatteMode mode)
  {
   impl_->settings.matteMode = mode;
  }

  const MatteStack& Layer2D::matteStack() const
  {
   return impl_->settings.matteStack;
  }

  void Layer2D::setMatteStack(const MatteStack& stack)
  {
   impl_->settings.matteStack = stack;
  }

}
