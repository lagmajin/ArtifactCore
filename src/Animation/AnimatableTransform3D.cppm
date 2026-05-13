module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <cmath>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Animation.Transform3D;



import Animation.Value;
import Math.Interpolate;
//import Graphics.CBuffer.Constants;

namespace ArtifactCore
{
using namespace Diligent;

namespace {

template <typename... AnimatableValues>
std::vector<RationalTime> collectUniqueKeyFrameTimes(
    const AnimatableValues &...values) {
  std::set<int64_t> frameSet;
  const auto collect = [&](const auto &animValue) {
    for (const auto &kf : animValue.getKeyFrames()) {
      frameSet.insert(kf.frame.framePosition());
    }
  };
  (collect(values), ...);

  std::vector<RationalTime> times;
  times.reserve(frameSet.size());
  for (const int64_t framePos : frameSet) {
    times.emplace_back(framePos, 24);
  }
  return times;
}

} // namespace

class AnimatableTransform3D::Impl
{
public:
  bool isZVisible = false;

  float initialX_ = 0, initialY_ = 0, initialZ_ = 0;
  float initialScaleX_ = 1, initialScaleY_ = 1;
  float initialRotation_ = 0;

  AnimatableValueT<float> x_;
  AnimatableValueT<float> y_;
  AnimatableValueT<float> z_;
  AnimatableValueT<float> rotation_;
  AnimatableValueT<float> scaleX_;
  AnimatableValueT<float> scaleY_;
  AnimatableValueT<float> anchorX_;
  AnimatableValueT<float> anchorY_;
  AnimatableValueT<float> anchorZ_;

  float currentX_ = 0.0f;
  float currentY_ = 0.0f;
  float currentZ_ = 0.0f;
  float currentRotation_ = 0.0f;
  float currentScaleX_ = 1.0f;
  float currentScaleY_ = 1.0f;

  Impl() = default;
  ~Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
};


 AnimatableTransform3D::AnimatableTransform3D() :impl_(new Impl())
 {

 }

 AnimatableTransform3D::~AnimatableTransform3D()
 {
  delete impl_;
 }

 AnimatableTransform3D::AnimatableTransform3D(const AnimatableTransform3D& other) : impl_(new Impl(*other.impl_)) {}
 AnimatableTransform3D& AnimatableTransform3D::operator=(const AnimatableTransform3D& other) {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 AnimatableTransform3D::AnimatableTransform3D(AnimatableTransform3D&& other) noexcept : impl_(other.impl_) {
  other.impl_ = new Impl();
 }
 AnimatableTransform3D& AnimatableTransform3D::operator=(AnimatableTransform3D&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = new Impl();
  }
  return *this;
 }

void AnimatableTransform3D::setInitalAngle(const RationalTime& time, float angle/*=0*/)
{
  impl_->initialRotation_ = angle;
  impl_->currentRotation_ = angle;
}

void AnimatableTransform3D::setInitialScale(const RationalTime& time, float xs, float ys)
{
  impl_->initialScaleX_ = xs;
  impl_->initialScaleY_ = ys;
  impl_->scaleX_.setCurrent(xs);
  impl_->scaleY_.setCurrent(ys);
  impl_->currentScaleX_ = xs;
  impl_->currentScaleY_ = ys;
}

void AnimatableTransform3D::setInitialPosition(const RationalTime& time, float px, float py)
{
  impl_->initialX_ = px;
  impl_->initialY_ = py;
  impl_->currentX_ = px;
  impl_->currentY_ = py;
}

void AnimatableTransform3D::setPosition(const RationalTime& time, float x, float y)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->x_.addKeyFrame(frame, x);
  impl_->y_.addKeyFrame(frame, y);
  impl_->currentX_ = impl_->initialX_ + x;
  impl_->currentY_ = impl_->initialY_ + y;
}

void AnimatableTransform3D::setPositionZ(const RationalTime& time, float z)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->z_.addKeyFrame(frame, z);
  impl_->currentZ_ = z;
}

void AnimatableTransform3D::setAnchor(const RationalTime& time, float x, float y, float z)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->anchorX_.addKeyFrame(frame, x);
  impl_->anchorY_.addKeyFrame(frame, y);
  impl_->anchorZ_.addKeyFrame(frame, z);
}

void AnimatableTransform3D::setRotation(const RationalTime& time, float degrees)
{
  FramePosition frame(time.rescaledTo(24));
  impl_->rotation_.addKeyFrame(frame, degrees);
  impl_->currentRotation_ = degrees;
}

float AnimatableTransform3D::positionX() const
{
  return impl_->currentX_;
}

float AnimatableTransform3D::positionY() const
{
  return impl_->currentY_;
}

float AnimatableTransform3D::positionZ() const
{
  return impl_->currentZ_;
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

float AnimatableTransform3D::anchorX() const
{
  return impl_->anchorX_.current();
}

float AnimatableTransform3D::anchorY() const
{
  return impl_->anchorY_.current();
}

float AnimatableTransform3D::anchorZ() const
{
  return impl_->anchorZ_.current();
}

// ============================================
// �V�K���\�b�h�FsetScale�i�L�[�t���[���Ή��j
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
// �V�K���\�b�h�F�����w��̃A�j���[�V������Ԓl�擾
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

float AnimatableTransform3D::positionZAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->z_.at(frame);
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

float AnimatableTransform3D::anchorXAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->anchorX_.at(frame);
}

float AnimatableTransform3D::anchorYAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->anchorY_.at(frame);
}

float AnimatableTransform3D::anchorZAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->anchorZ_.at(frame);
}

// ============================================
// 4x4�ϊ��s��̐����i���ݒl����j
// ============================================
float4x4 AnimatableTransform3D::getMatrix() const
{
  // �ϊ������FScale �� Rotation �� Translation
  float4x4 matrix = float4x4::Identity();
  
  // 1. �X�P�[���s��
  float4x4 scaleMatrix = float4x4::Scale(impl_->currentScaleX_, impl_->currentScaleY_, 1.0f);
  
  // 2. ��]�s��iZ����]�A�x�����烉�W�A���֕ϊ��j
  float radians = impl_->currentRotation_ * 3.14159265358979323846f / 180.0f;
  float cosR = std::cos(radians);
  float sinR = std::sin(radians);
  
  float4x4 rotationMatrix = float4x4::Identity();
  rotationMatrix.m00 = cosR;
  rotationMatrix.m01 = -sinR;
  rotationMatrix.m10 = sinR;
  rotationMatrix.m11 = cosR;
  
  // 3. sړs
  float4x4 translationMatrix = float4x4::Translation(impl_->currentX_, impl_->currentY_, 0.0f);
  
  // 4. sFTranslation * Rotation * Scale
  matrix = translationMatrix * rotationMatrix * scaleMatrix;
  
  return matrix;
}

float4x4 AnimatableTransform3D::getAllMatrix() const
{
  float4x4 scaleMatrix = float4x4::Scale(impl_->currentScaleX_, impl_->currentScaleY_, 1.0f);

  float radians = impl_->currentRotation_ * 3.14159265358979323846f / 180.0f;
  float cosR = std::cos(radians);
  float sinR = std::sin(radians);

  float4x4 rotationMatrix = float4x4::Identity();
  rotationMatrix.m00 = cosR;
  rotationMatrix.m01 = -sinR;
  rotationMatrix.m10 = sinR;
  rotationMatrix.m11 = cosR;

  float ax = impl_->anchorX_.current();
  float ay = impl_->anchorY_.current();
  float az = impl_->anchorZ_.current();

  float4x4 anchorMatrix = float4x4::Translation(-ax, -ay, -az);
  float4x4 translationMatrix = float4x4::Translation(impl_->currentX_, impl_->currentY_, impl_->currentZ_);

  return translationMatrix * rotationMatrix * scaleMatrix * anchorMatrix;
}

// ============================================
// 4x4�ϊ��s��̐����i�w�莞���̃A�j���[�V�����l����j
// ============================================
float4x4 AnimatableTransform3D::getMatrixAt(const RationalTime& time) const
{
  // �w�莞���̒l���擾
  FramePosition frame(time.rescaledTo(24));
  
  float px = impl_->x_.at(frame);
  float py = impl_->y_.at(frame);
  float rot = impl_->rotation_.at(frame);
  float sx = impl_->scaleX_.at(frame);
  float sy = impl_->scaleY_.at(frame);
  
  // �ϊ������FScale �� Rotation �� Translation
  float4x4 matrix = float4x4::Identity();
  
  // 1. �X�P�[���s��
  float4x4 scaleMatrix = float4x4::Scale(sx, sy, 1.0f);
  
  // 2. ��]�s��iZ����]�A�x�����烉�W�A���֕ϊ��j
  float radians = rot * 3.14159265358979323846f / 180.0f;
  float cosR = std::cos(radians);
  float sinR = std::sin(radians);
  
  float4x4 rotationMatrix = float4x4::Identity();
  rotationMatrix.m00 = cosR;
  rotationMatrix.m01 = -sinR;
  rotationMatrix.m10 = sinR;
  rotationMatrix.m11 = cosR;
  
  // 3. s�r�s��
  float4x4 translationMatrix = float4x4::Translation(px, py, 0.0f);
  
  // 4. s����Translation * Rotation * Scale
  matrix = translationMatrix * rotationMatrix * scaleMatrix;
  
  return matrix;
}

float4x4 AnimatableTransform3D::getAllMatrixAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));

  // 1. Get Offset values (Animation Keyframes)
  float ox = impl_->x_.at(frame);
  float oy = impl_->y_.at(frame);
  float oz = impl_->z_.at(frame);
  float orot = impl_->rotation_.at(frame);
  float osx = impl_->scaleX_.at(frame);
  float osy = impl_->scaleY_.at(frame);
  float ax = impl_->anchorX_.at(frame);
  float ay = impl_->anchorY_.at(frame);
  float az = impl_->anchorZ_.at(frame);

  // 2. Combine Baseline (Layout) + Offset (Animation)
  float finalX = impl_->initialX_ + ox;
  float finalY = impl_->initialY_ + oy;
  float finalZ = impl_->initialZ_ + oz;
  float finalRot = impl_->initialRotation_ + orot;
  float finalScaleX = impl_->initialScaleX_ * osx; // Multiplicative scale
  float finalScaleY = impl_->initialScaleY_ * osy;

  // 3. Construct Final Matrix (T * R * S * A)
  float4x4 scaleMatrix = float4x4::Scale(finalScaleX, finalScaleY, 1.0f);

  float radians = finalRot * 3.14159265358979323846f / 180.0f;
  float cosR = std::cos(radians);
  float sinR = std::sin(radians);

  float4x4 rotationMatrix = float4x4::Identity();
  rotationMatrix.m00 = cosR;
  rotationMatrix.m01 = -sinR;
  rotationMatrix.m10 = sinR;
  rotationMatrix.m11 = cosR;

  float4x4 anchorMatrix = float4x4::Translation(-ax, -ay, -az);
  float4x4 translationMatrix = float4x4::Translation(finalX, finalY, finalZ);

  return translationMatrix * rotationMatrix * scaleMatrix * anchorMatrix;
}

Transform3DSnapshot AnimatableTransform3D::snapshot() const
{
  Transform3DSnapshot snapshot;
  snapshot.positionX = impl_->currentX_;
  snapshot.positionY = impl_->currentY_;
  snapshot.positionZ = impl_->currentZ_;
  snapshot.rotation = impl_->currentRotation_;
  snapshot.scaleX = impl_->currentScaleX_;
  snapshot.scaleY = impl_->currentScaleY_;
  snapshot.anchorX = impl_->anchorX_.current();
  snapshot.anchorY = impl_->anchorY_.current();
  snapshot.anchorZ = impl_->anchorZ_.current();
  snapshot.isZVisible = impl_->isZVisible;
  snapshot.matrix = getMatrix();
  snapshot.matrixWithAnchor = getAllMatrix();
  snapshot.anchorCanvasPosition = anchorPosition();
  return snapshot;
}

Transform3DSnapshot AnimatableTransform3D::snapshotAt(const RationalTime& time) const
{
  Transform3DSnapshot snapshot;
  FramePosition frame(time.rescaledTo(24));
  snapshot.positionX = impl_->initialX_ + impl_->x_.at(frame);
  snapshot.positionY = impl_->initialY_ + impl_->y_.at(frame);
  snapshot.positionZ = impl_->initialZ_ + impl_->z_.at(frame);
  snapshot.rotation = impl_->initialRotation_ + impl_->rotation_.at(frame);
  snapshot.scaleX = impl_->initialScaleX_ * impl_->scaleX_.at(frame);
  snapshot.scaleY = impl_->initialScaleY_ * impl_->scaleY_.at(frame);
  snapshot.anchorX = impl_->anchorX_.at(frame);
  snapshot.anchorY = impl_->anchorY_.at(frame);
  snapshot.anchorZ = impl_->anchorZ_.at(frame);
  snapshot.isZVisible = impl_->isZVisible;
  snapshot.matrix = getMatrixAt(time);
  snapshot.matrixWithAnchor = getAllMatrixAt(time);
  snapshot.anchorCanvasPosition = anchorPositionAt(time);
  return snapshot;
}

float2 AnimatableTransform3D::anchorPosition() const
{
  const auto matrix = getAllMatrix();
  return float2{matrix.m30, matrix.m31};
}

float2 AnimatableTransform3D::anchorPositionAt(const RationalTime& time) const
{
  const auto matrix = getAllMatrixAt(time);
  return float2{matrix.m30, matrix.m31};
}

// ============================================
// �L�[�t���[���Ǘ��@�\�̎���
// ============================================

// Position �L�[�t���[���Ǘ�
bool AnimatableTransform3D::hasPositionKeyFrameAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->x_.hasKeyFrameAt(frame) || impl_->y_.hasKeyFrameAt(frame);
}

bool AnimatableTransform3D::setPositionKeyFrameValueAt(const RationalTime& time,
                                                       float x, float y)
{
  FramePosition frame(time.rescaledTo(24));
  const bool xOk = impl_->x_.setKeyFrameValueAt(frame, x);
  const bool yOk = impl_->y_.setKeyFrameValueAt(frame, y);
  if (!xOk || !yOk) {
    if (!xOk) {
      impl_->x_.addKeyFrame(frame, x);
    }
    if (!yOk) {
      impl_->y_.addKeyFrame(frame, y);
    }
  }
  impl_->currentX_ = impl_->initialX_ + x;
  impl_->currentY_ = impl_->initialY_ + y;
  return true;
}

bool AnimatableTransform3D::setPositionKeyFrameInterpolationAt(
    const RationalTime& time, InterpolationType xInterpolation,
    InterpolationType yInterpolation)
{
  FramePosition frame(time.rescaledTo(24));
  const bool xOk = impl_->x_.setKeyFrameInterpolationAt(frame, xInterpolation);
  const bool yOk = impl_->y_.setKeyFrameInterpolationAt(frame, yInterpolation);
  return xOk || yOk;
}

InterpolationType AnimatableTransform3D::positionXKeyFrameInterpolationAt(
    const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->x_.getKeyFrameInterpolationAt(frame);
}

InterpolationType AnimatableTransform3D::positionYKeyFrameInterpolationAt(
    const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(24));
  return impl_->y_.getKeyFrameInterpolationAt(frame);
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
  // X �� Y �̃L�[�t���[�����̍ő�l��Ԃ��i�ʏ�͓����͂��j
  return std::max(impl_->x_.getKeyFrameCount(), impl_->y_.getKeyFrameCount());
}

std::vector<RationalTime> AnimatableTransform3D::getPositionKeyFrameTimes() const
{
  return collectUniqueKeyFrameTimes(impl_->x_, impl_->y_);
}

// Rotation �L�[�t���[���Ǘ�
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
  return collectUniqueKeyFrameTimes(impl_->rotation_);
}

// Scale �L�[�t���[���Ǘ�
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
  return collectUniqueKeyFrameTimes(impl_->scaleX_, impl_->scaleY_);
}

std::vector<RationalTime> AnimatableTransform3D::getAllKeyFrameTimes() const
{
  return collectUniqueKeyFrameTimes(impl_->x_, impl_->y_, impl_->z_,
                                    impl_->rotation_, impl_->scaleX_,
                                    impl_->scaleY_, impl_->anchorX_,
                                    impl_->anchorY_, impl_->anchorZ_);
}

// ���ׂăN���A
void AnimatableTransform3D::clearAllKeyFrames()
{
  clearPositionKeyFrames();
  clearRotationKeyFrames();
  clearScaleKeyFrames();
}

}
