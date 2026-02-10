module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <cmath>

module Animation.Transform3D;
import std;
import Animation.Value;
import Math.Interpolate;
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

// ============================================
// キーフレーム管理機能の実装
// ============================================

// Position キーフレーム管理
bool AnimatableTransform3D::hasPositionKeyFrameAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->x_.hasKeyFrameAt(frame) || impl_->y_.hasKeyFrameAt(frame);
}

void AnimatableTransform3D::removePositionKeyFrameAt(const RationalTime& time)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->x_.removeKeyFrameAt(frame);
  impl_->y_.removeKeyFrameAt(frame);
}

void AnimatableTransform3D::clearPositionKeyFrames()
{
  impl_->x_.clearKeyFrames();
  impl_->y_.clearKeyFrames();
}

size_t AnimatableTransform3D::getPositionKeyFrameCount() const
{
  // X と Y のキーフレーム数の最大値を返す（通常は同じはず）
  return std::max(impl_->x_.getKeyFrameCount(), impl_->y_.getKeyFrameCount());
}

std::vector<RationalTime> AnimatableTransform3D::getPositionKeyFrameTimes() const
{
  std::set<int64_t> frameSet;
  
  // X のキーフレーム時刻を追加
  for (const auto& kf : impl_->x_.getKeyFrames()) {
    frameSet.insert(kf.frame.framePosition());
  }
  
  // Y のキーフレーム時刻を追加
  for (const auto& kf : impl_->y_.getKeyFrames()) {
    frameSet.insert(kf.frame.framePosition());
  }
  
  // RationalTime に変換
  std::vector<RationalTime> times;
  for (int64_t framePos : frameSet) {
    times.push_back(RationalTime(framePos, 24));
  }
  
  return times;
}

void AnimatableTransform3D::movePositionKeyFrame(const RationalTime& from, const RationalTime& to)
{
  FramePosition fromFrame(from.rescaledTo(24));
  FramePosition toFrame(to.rescaledTo(24));
  impl_->x_.moveKeyFrame(fromFrame, toFrame);
  impl_->y_.moveKeyFrame(fromFrame, toFrame);
}

void AnimatableTransform3D::moveRotationKeyFrame(const RationalTime& from, const RationalTime& to)
{
  FramePosition fromFrame(from.rescaledTo(24));
  FramePosition toFrame(to.rescaledTo(24));
  impl_->rotation_.moveKeyFrame(fromFrame, toFrame);
}

void AnimatableTransform3D::moveScaleKeyFrame(const RationalTime& from, const RationalTime& to)
{
  FramePosition fromFrame(from.rescaledTo(24));
  FramePosition toFrame(to.rescaledTo(24));
  impl_->scaleX_.moveKeyFrame(fromFrame, toFrame);
  impl_->scaleY_.moveKeyFrame(fromFrame, toFrame);
}

void AnimatableTransform3D::setPositionKeyFrameInterpolationAt(const RationalTime& time, InterpolationType interpolation)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->x_.setKeyFrameInterpolationAt(frame, interpolation);
  impl_->y_.setKeyFrameInterpolationAt(frame, interpolation);
}

void AnimatableTransform3D::setRotationKeyFrameInterpolationAt(const RationalTime& time, InterpolationType interpolation)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->rotation_.setKeyFrameInterpolationAt(frame, interpolation);
}

void AnimatableTransform3D::setScaleKeyFrameInterpolationAt(const RationalTime& time, InterpolationType interpolation)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->scaleX_.setKeyFrameInterpolationAt(frame, interpolation);
  impl_->scaleY_.setKeyFrameInterpolationAt(frame, interpolation);
}

InterpolationType AnimatableTransform3D::positionKeyFrameInterpolationAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  if (impl_->x_.hasKeyFrameAt(frame)) {
    return impl_->x_.getKeyFrameInterpolationAt(frame);
  }
  if (impl_->y_.hasKeyFrameAt(frame)) {
    return impl_->y_.getKeyFrameInterpolationAt(frame);
  }
  return InterpolationType::Linear;
}

InterpolationType AnimatableTransform3D::rotationKeyFrameInterpolationAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->rotation_.getKeyFrameInterpolationAt(frame);
}

InterpolationType AnimatableTransform3D::scaleKeyFrameInterpolationAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  if (impl_->scaleX_.hasKeyFrameAt(frame)) {
    return impl_->scaleX_.getKeyFrameInterpolationAt(frame);
  }
  if (impl_->scaleY_.hasKeyFrameAt(frame)) {
    return impl_->scaleY_.getKeyFrameInterpolationAt(frame);
  }
  return InterpolationType::Linear;
}

// Rotation キーフレーム管理
bool AnimatableTransform3D::hasRotationKeyFrameAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->rotation_.hasKeyFrameAt(frame);
}

void AnimatableTransform3D::removeRotationKeyFrameAt(const RationalTime& time)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->rotation_.removeKeyFrameAt(frame);
}

void AnimatableTransform3D::clearRotationKeyFrames()
{
  impl_->rotation_.clearKeyFrames();
}

size_t AnimatableTransform3D::getRotationKeyFrameCount() const
{
  return impl_->rotation_.getKeyFrameCount();
}

std::vector<RationalTime> AnimatableTransform3D::getRotationKeyFrameTimes() const
{
  std::vector<RationalTime> times;
  
  for (const auto& kf : impl_->rotation_.getKeyFrames()) {
    times.push_back(RationalTime(kf.frame.framePosition(), 24));
  }
  
  return times;
}

// Scale キーフレーム管理
bool AnimatableTransform3D::hasScaleKeyFrameAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->scaleX_.hasKeyFrameAt(frame) || impl_->scaleY_.hasKeyFrameAt(frame);
}

void AnimatableTransform3D::removeScaleKeyFrameAt(const RationalTime& time)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->scaleX_.removeKeyFrameAt(frame);
  impl_->scaleY_.removeKeyFrameAt(frame);
}

void AnimatableTransform3D::clearScaleKeyFrames()
{
  impl_->scaleX_.clearKeyFrames();
  impl_->scaleY_.clearKeyFrames();
}

size_t AnimatableTransform3D::getScaleKeyFrameCount() const
{
  return std::max(impl_->scaleX_.getKeyFrameCount(), impl_->scaleY_.getKeyFrameCount());
}

std::vector<RationalTime> AnimatableTransform3D::getScaleKeyFrameTimes() const
{
  std::set<int64_t> frameSet;
  
  // ScaleX のキーフレーム時刻を追加
  for (const auto& kf : impl_->scaleX_.getKeyFrames()) {
    frameSet.insert(kf.frame.framePosition());
  }
  
  // ScaleY のキーフレーム時刻を追加
  for (const auto& kf : impl_->scaleY_.getKeyFrames()) {
    frameSet.insert(kf.frame.framePosition());
  }
  
  // RationalTime に変換
  std::vector<RationalTime> times;
  for (int64_t framePos : frameSet) {
    times.push_back(RationalTime(framePos, 24));
  }
  
  return times;
}

// すべてクリア
void AnimatableTransform3D::clearAllKeyFrames()
{
  clearPositionKeyFrames();
  clearRotationKeyFrames();
  clearScaleKeyFrames();
}

};