module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <cmath>

module Animation.Transform3D;
import std;
import Animation.Value;
//import Graphics.CBuffer.Constants;

namespace ArtifactCore
{
using namespace Diligent;
class AnimatableTransform3D::Impl
{
private:
   
public:
  bool isZVisible = false;
  float initial_x = 0;
  float initial_y = 0;
  
  // アニメーション対応の値
  AnimatableValueT<float> x_;
  AnimatableValueT<float> y_;
  AnimatableValueT<float> z_;
  AnimatableValueT<float> rotation_;
  AnimatableValueT<float> scaleX_;
  AnimatableValueT<float> scaleY_;
  AnimatableValueT<float> anchorX_;
  AnimatableValueT<float> anchorY_;
  AnimatableValueT<float> anchorZ_;
  
  // 現在値（キャッシュ）
  float positionX_ = 0.0f;
  float positionY_ = 0.0f;
  float currentRotation_ = 0.0f;
  float currentScaleX_ = 1.0f;
  float currentScaleY_ = 1.0f;
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
  impl_->currentRotation_ = angle;
  impl_->rotation_.setCurrent(angle);
}

void AnimatableTransform3D::setInitialScale(const RationalTime& time, float xs, float ys)
{
  impl_->currentScaleX_ = xs;
  impl_->currentScaleY_ = ys;
  impl_->scaleX_.setCurrent(xs);
  impl_->scaleY_.setCurrent(ys);
}

void AnimatableTransform3D::setInitialPosition(const RationalTime& time, float px, float py)
{
  impl_->positionX_ = px;
  impl_->positionY_ = py;
}

void AnimatableTransform3D::setPosition(const RationalTime& time, float x, float y)
{
  // RationalTimeをフレーム位置に変換（24fps想定）
  FramePosition frame(time.rescaledTo(24));
  impl_->x_.addKeyFrame(frame, x);
  impl_->y_.addKeyFrame(frame, y);
  impl_->positionX_ = x;
  impl_->positionY_ = y;
}

void AnimatableTransform3D::setRotation(const RationalTime& time, float degrees)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->rotation_.addKeyFrame(frame, degrees);
  impl_->currentRotation_ = degrees;
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
  return impl_->currentRotation_;
}

float AnimatableTransform3D::scaleX() const
{
  return impl_->currentScaleX_;
}

float AnimatableTransform3D::scaleY() const
{
  return impl_->currentScaleY_;
}

// ============================================
// 新規メソッド：setScale（キーフレーム対応）
// ============================================
void AnimatableTransform3D::setScale(const RationalTime& time, float xs, float ys)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->scaleX_.addKeyFrame(frame, xs);
  impl_->scaleY_.addKeyFrame(frame, ys);
  impl_->currentScaleX_ = xs;
  impl_->currentScaleY_ = ys;
}

// ============================================
// 新規メソッド：時刻指定のアニメーション補間値取得
// ============================================
float AnimatableTransform3D::positionXAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->x_.at(frame);
}

float AnimatableTransform3D::positionYAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->y_.at(frame);
}

float AnimatableTransform3D::rotationAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->rotation_.at(frame);
}

float AnimatableTransform3D::scaleXAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->scaleX_.at(frame);
}

float AnimatableTransform3D::scaleYAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->scaleY_.at(frame);
}

void AnimatableTransform3D::setUserScale(const RationalTime& time, float xs, float ys)
{
  // setScale と同じ実装
  setScale(time, xs, ys);
}

// ============================================
// 4x4変換行列の生成（現在値から）
// ============================================
float4x4 AnimatableTransform3D::getMatrix() const
{
  // 変換順序：Scale → Rotation → Translation
  float4x4 matrix = float4x4::Identity();
  
  // 1. スケール行列
  float4x4 scaleMatrix = float4x4::Scale(impl_->currentScaleX_, impl_->currentScaleY_, 1.0f);
  
  // 2. 回転行列（Z軸回転、度数からラジアンへ変換）
  float radians = impl_->currentRotation_ * 3.14159265358979323846f / 180.0f;
  float cosR = std::cos(radians);
  float sinR = std::sin(radians);
  
  float4x4 rotationMatrix = float4x4::Identity();
  rotationMatrix.m00 = cosR;
  rotationMatrix.m01 = -sinR;
  rotationMatrix.m10 = sinR;
  rotationMatrix.m11 = cosR;
  
  // 3. 平行移動行列
  float4x4 translationMatrix = float4x4::Translation(impl_->positionX_, impl_->positionY_, 0.0f);
  
  // 4. 行列を結合：Translation * Rotation * Scale
  matrix = translationMatrix * rotationMatrix * scaleMatrix;
  
  return matrix;
}

// ============================================
// 4x4変換行列の生成（指定時刻のアニメーション値から）
// ============================================
float4x4 AnimatableTransform3D::getMatrixAt(const RationalTime& time) const
{
  // 指定時刻の値を取得
  FramePosition frame(time.rescaledTo(24));
  
  float px = impl_->x_.at(frame);
  float py = impl_->y_.at(frame);
  float rot = impl_->rotation_.at(frame);
  float sx = impl_->scaleX_.at(frame);
  float sy = impl_->scaleY_.at(frame);
  
  // 変換順序：Scale → Rotation → Translation
  float4x4 matrix = float4x4::Identity();
  
  // 1. スケール行列
  float4x4 scaleMatrix = float4x4::Scale(sx, sy, 1.0f);
  
  // 2. 回転行列（Z軸回転、度数からラジアンへ変換）
  float radians = rot * 3.14159265358979323846f / 180.0f;
  float cosR = std::cos(radians);
  float sinR = std::sin(radians);
  
  float4x4 rotationMatrix = float4x4::Identity();
  rotationMatrix.m00 = cosR;
  rotationMatrix.m01 = -sinR;
  rotationMatrix.m10 = sinR;
  rotationMatrix.m11 = cosR;
  
  // 3. 平行移動行列
  float4x4 translationMatrix = float4x4::Translation(px, py, 0.0f);
  
  // 4. 行列を結合：Translation * Rotation * Scale
  matrix = translationMatrix * rotationMatrix * scaleMatrix;
  
  return matrix;
}

};