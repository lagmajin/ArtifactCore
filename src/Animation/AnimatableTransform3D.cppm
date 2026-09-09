module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <optional>
#include <vector>
#include <QPointF>
#include <QVariant>
#include <QString>

module Animation.Transform3D;

import Animation.Value;
import Frame.Position;
import Property.Types;
import Math.Interpolate;
import Container.NamedVector;
//import Graphics.CBuffer.Constants;

namespace ArtifactCore
{
using namespace Diligent;

namespace {

// Compatibility view over the authoritative property channel. There is no
// second key vector: legacy frame-based callers and property editors mutate
// the same AbstractProperty. Position offsets are converted at this boundary.
class TransformPropertyChannel {
public:
  TransformPropertyChannel() : property_(makeShared<AbstractProperty>()) {
    property_->setType(PropertyType::Float);
    property_->setAnimatable(true);
    property_->setValue(0.0f);
  }
  TransformPropertyChannel(const TransformPropertyChannel& other)
      : property_(makeShared<AbstractProperty>(*other.property_)),
        scale_(other.scale_), offset_(other.offset_) {}
  TransformPropertyChannel& operator=(const TransformPropertyChannel& other) {
    if (this != &other) {
      *property_ = *other.property_;
      scale_ = other.scale_;
      offset_ = other.offset_;
    }
    return *this;
  }
  SharedPtr<AbstractProperty> property() const { return property_; }
  void setTimeScale(int64_t scale) { scale_ = scale; }
  void setOffset(float offset) {
    const float delta = offset - offset_;
    if (delta == 0.0f) return;
    property_->setValue(property_->getValue().toFloat() + delta);
    for (const auto& key : property_->getKeyFrames()) {
      write(key, key.value.toFloat() + delta, key.interpolation);
    }
    offset_ = offset;
  }
  float current() const { return property_->getValue().toFloat() - offset_; }
  void setCurrent(float value) { property_->setValue(value + offset_); }
  float at(const FramePosition& frame) const {
    return property_->interpolateValue(time(frame)).toFloat() - offset_;
  }
  void addKeyFrame(const FramePosition& frame, float value) {
    if (!setKeyFrameValueAt(frame, value))
      property_->addKeyFrame(time(frame), value + offset_);
  }
  bool hasKeyFrameAt(const FramePosition& frame) const {
    return property_->hasKeyFrameAt(time(frame));
  }
  bool setKeyFrameValueAt(const FramePosition& frame, float value) {
    for (const auto& key : property_->getKeyFrames()) {
      if (key.time == time(frame)) {
        write(key, value + offset_, key.interpolation);
        return true;
      }
    }
    return false;
  }
  bool setKeyFrameInterpolationAt(const FramePosition& frame, InterpolationType type) {
    for (const auto& key : property_->getKeyFrames()) {
      if (key.time == time(frame)) {
        write(key, key.value.toFloat(), type);
        return true;
      }
    }
    return false;
  }
  InterpolationType getKeyFrameInterpolationAt(const FramePosition& frame) const {
    for (const auto& key : property_->getKeyFrames())
      if (key.time == time(frame)) return key.interpolation;
    return InterpolationType::Linear;
  }
  void removeKeyFrameAt(const FramePosition& frame) { property_->removeKeyFrame(time(frame)); }
  void clearKeyFrames() { property_->clearKeyFrames(); }
  size_t getKeyFrameCount() const { return property_->getKeyFrames().size(); }
  auto getKeyFrames() const {
    auto result = makeNamedVector<KeyFrameT<float>>(
        ContainerName{"TransformPropertyKeyframeView"});
    for (const auto& key : property_->getKeyFrames())
      result.add(KeyFrameT<float>{FramePosition(key.time.rescaledTo(scale_)),
                                 key.value.toFloat() - offset_, key.interpolation});
    return result.toStdVector();
  }
private:
  RationalTime time(const FramePosition& frame) const {
    return RationalTime(frame.framePosition(), scale_);
  }
  void write(const KeyFrame& key, float value, InterpolationType type) {
    property_->addKeyFrame(key.time, value, type,
        key.cp1_x, key.cp1_y, key.cp2_x, key.cp2_y, key.roving);
    property_->setKeyFrameAnchorAt(key.time, key.anchor);
    property_->setKeyFrameColorLabelAt(key.time, key.colorLabel);
  }
  SharedPtr<AbstractProperty> property_;
  int64_t scale_ = 24;
  float offset_ = 0.0f;
};

template <typename... AnimatableValues>
std::vector<RationalTime> collectUniqueKeyFrameTimes(
    int64_t timeScale, const AnimatableValues &...values) {
  std::set<int64_t> frameSet;
  const auto collect = [&](const auto &animValue) {
    for (const auto &kf : animValue.getKeyFrames()) {
      frameSet.insert(kf.frame.framePosition());
    }
  };
  (collect(values), ...);

  NamedVector<RationalTime> times{
      makeNamedVector<RationalTime>(ContainerName{"Transform3DKeyframeTimes"})};
  times.reserve(frameSet.size());
  for (const int64_t framePos : frameSet) {
    times.add(RationalTime{framePos, timeScale});
  }
  return times.toStdVector();
}

} // namespace

static float4x4 makeEulerRotation(float rotationX, float rotationY,
                                   float rotationZ)
{
  constexpr float degreesToRadians =
      3.14159265358979323846f / 180.0f;
  const float x = rotationX * degreesToRadians;
  const float y = rotationY * degreesToRadians;
  const float z = rotationZ * degreesToRadians;
  const float cx = std::cos(x);
  const float sx = std::sin(x);
  const float cy = std::cos(y);
  const float sy = std::sin(y);
  const float cz = std::cos(z);
  const float sz = std::sin(z);

  float4x4 rotationXMatrix = float4x4::Identity();
  rotationXMatrix.m11 = cx;
  rotationXMatrix.m12 = -sx;
  rotationXMatrix.m21 = sx;
  rotationXMatrix.m22 = cx;

  float4x4 rotationYMatrix = float4x4::Identity();
  rotationYMatrix.m00 = cy;
  rotationYMatrix.m02 = sy;
  rotationYMatrix.m20 = -sy;
  rotationYMatrix.m22 = cy;

  float4x4 rotationZMatrix = float4x4::Identity();
  rotationZMatrix.m00 = cz;
  rotationZMatrix.m01 = -sz;
  rotationZMatrix.m10 = sz;
  rotationZMatrix.m11 = cz;

  return rotationZMatrix * rotationYMatrix * rotationXMatrix;
}

class AnimatableTransform3D::Impl
{
public:
  bool isZVisible = false;
  AutoOrientMode autoOrientMode_ = AutoOrientMode::Off;

  // キー格納・評価に使う時刻スケール。従来は24固定だったため非24fpsで
  // 量子化ズレ(重複バケット・キー潰れ)が起きる。レイヤー側がコンポfpsを
  // 設定するまでの既定値は24(従来互換)。
  int64_t timeScale_ = 24;

  float initialX_ = 0, initialY_ = 0, initialZ_ = 0;
  float initialScaleX_ = 1, initialScaleY_ = 1;
  float initialScaleZ_ = 1;
  float initialRotation_ = 0;

  TransformPropertyChannel x_;
  TransformPropertyChannel y_;
  TransformPropertyChannel z_;
  TransformPropertyChannel rotation_;
  TransformPropertyChannel rotationX_;
  TransformPropertyChannel rotationY_;
  TransformPropertyChannel scaleX_;
  TransformPropertyChannel scaleY_;
  TransformPropertyChannel scaleZ_;
  TransformPropertyChannel anchorX_;
  TransformPropertyChannel anchorY_;
  TransformPropertyChannel anchorZ_;
  std::map<int64_t, PositionSpatialTangents> positionSpatialTangents_;

  float currentX_ = 0.0f;
  float currentY_ = 0.0f;
  float currentZ_ = 0.0f;
  float currentRotation_ = 0.0f;
  float currentRotationX_ = 0.0f;
  float currentRotationY_ = 0.0f;
  float currentScaleX_ = 1.0f;
  float currentScaleY_ = 1.0f;
  float currentScaleZ_ = 1.0f;

  std::optional<float2> spatialPositionAt(const FramePosition& frame) const {
    const auto xFrames = x_.getKeyFrames();
    const auto yFrames = y_.getKeyFrames();
    if (xFrames.size() < 2 || yFrames.size() < 2) {
      return std::nullopt;
    }
    const auto nextX = std::lower_bound(
        xFrames.begin(), xFrames.end(), frame,
        [](const auto& keyframe, const FramePosition& value) {
          return keyframe.frame < value;
        });
    if (nextX == xFrames.begin() || nextX == xFrames.end() ||
        nextX->frame == frame) {
      return std::nullopt;
    }
    const auto previousX = std::prev(nextX);
    const auto findY = [&yFrames](const FramePosition& keyframe) {
      return std::find_if(yFrames.begin(), yFrames.end(),
                          [&keyframe](const auto& candidate) {
                            return candidate.frame == keyframe;
                          });
    };
    const auto previousY = findY(previousX->frame);
    const auto nextY = findY(nextX->frame);
    if (previousY == yFrames.end() || nextY == yFrames.end()) {
      return std::nullopt;
    }
    if (previousX->interpolation == InterpolationType::Constant ||
        previousY->interpolation == InterpolationType::Constant) {
      return std::nullopt;
    }
    const auto previousTangent =
        positionSpatialTangents_.find(previousX->frame.framePosition());
    const auto nextTangent =
        positionSpatialTangents_.find(nextX->frame.framePosition());
    if (previousTangent == positionSpatialTangents_.end() &&
        nextTangent == positionSpatialTangents_.end()) return std::nullopt;
    const float duration = static_cast<float>(
        nextX->frame.framePosition() - previousX->frame.framePosition());
    if (duration <= 0.0f) {
      return std::nullopt;
    }
    const float t = std::clamp(
        static_cast<float>(frame.framePosition() -
                           previousX->frame.framePosition()) / duration,
        0.0f, 1.0f);
    const float u = 1.0f - t;
    const float2 p0{previousX->value, previousY->value};
    const float2 p3{nextX->value, nextY->value};
    const auto clampAutoTangent = [](float2 tangent, float maxLength) {
      const float lengthSquared = tangent.x * tangent.x + tangent.y * tangent.y;
      const float maxLengthSquared = maxLength * maxLength;
      if (lengthSquared > maxLengthSquared && lengthSquared > 1.0e-8f) {
        const float scale = maxLength / std::sqrt(lengthSquared);
        tangent.x *= scale;
        tangent.y *= scale;
      }
      return tangent;
    };
    const float segmentLength = std::hypot(p3.x - p0.x, p3.y - p0.y);
    const float maxAutoTangentLength = segmentLength * 0.5f;

    // Match an Auto Bezier spatial path when no manual handle is stored.
    // Interior handles follow the neighbouring keys (Catmull-Rom converted
    // to cubic Bezier); endpoints follow the first/last segment direction.
    float2 autoOut{(p3.x - p0.x) / 3.0f, (p3.y - p0.y) / 3.0f};
    if (previousX != xFrames.begin()) {
      const auto beforeX = std::prev(previousX);
      const auto beforeY = findY(beforeX->frame);
      if (beforeY != yFrames.end()) {
        autoOut = {(p3.x - beforeX->value) / 6.0f,
                   (p3.y - beforeY->value) / 6.0f};
      }
    }
    autoOut = clampAutoTangent(autoOut, maxAutoTangentLength);

    float2 autoIn{(p0.x - p3.x) / 3.0f, (p0.y - p3.y) / 3.0f};
    const auto afterX = std::next(nextX);
    if (afterX != xFrames.end()) {
      const auto afterY = findY(afterX->frame);
      if (afterY != yFrames.end()) {
        autoIn = {(p0.x - afterX->value) / 6.0f,
                  (p0.y - afterY->value) / 6.0f};
      }
    }
    autoIn = clampAutoTangent(autoIn, maxAutoTangentLength);

    const float2 out = previousTangent == positionSpatialTangents_.end()
                           ? autoOut
                           : previousTangent->second.outTangent;
    const float2 in = nextTangent == positionSpatialTangents_.end()
                          ? autoIn
                          : nextTangent->second.inTangent;
    const float2 p1{p0.x + out.x, p0.y + out.y};
    const float2 p2{p3.x + in.x, p3.y + in.y};
    return float2{
        u * u * u * p0.x + 3.0f * u * u * t * p1.x +
            3.0f * u * t * t * p2.x + t * t * t * p3.x,
        u * u * u * p0.y + 3.0f * u * u * t * p1.y +
            3.0f * u * t * t * p2.y + t * t * t * p3.y};
  }

  Impl() {
    x_.property()->setName(QStringLiteral("transform.position.x"));
    y_.property()->setName(QStringLiteral("transform.position.y"));
    z_.property()->setName(QStringLiteral("transform.position.z"));
    rotation_.property()->setName(QStringLiteral("transform.rotation"));
    rotationX_.property()->setName(QStringLiteral("transform.rotation.x"));
    rotationY_.property()->setName(QStringLiteral("transform.rotation.y"));
    scaleX_.property()->setName(QStringLiteral("transform.scale.x"));
    scaleY_.property()->setName(QStringLiteral("transform.scale.y"));
    scaleZ_.property()->setName(QStringLiteral("transform.scale.z"));
    anchorX_.property()->setName(QStringLiteral("transform.anchor.x"));
    anchorY_.property()->setName(QStringLiteral("transform.anchor.y"));
    anchorZ_.property()->setName(QStringLiteral("transform.anchor.z"));
    // Scale defaults to 1.0f so "no keyframes" still means "unchanged size".
    scaleX_.setCurrent(1.0f);
    scaleY_.setCurrent(1.0f);
    scaleZ_.setCurrent(1.0f);
  }
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
    // Keep exported property handles stable for already-bound editor rows.
    *impl_ = *other.impl_;
    *other.impl_ = Impl{};
   }
   return *this;
  }

 void AnimatableTransform3D::setKeyframeTimeScale(int64_t scale)
 {
  const auto nextScale = std::max<int64_t>(1, scale);
  if (nextScale != impl_->timeScale_) {
    decltype(impl_->positionSpatialTangents_) remapped;
    for (const auto& [frame, tangent] : impl_->positionSpatialTangents_)
      remapped[RationalTime(frame, impl_->timeScale_).rescaledTo(nextScale)] = tangent;
    impl_->positionSpatialTangents_ = std::move(remapped);
  }
  impl_->timeScale_ = nextScale;
  impl_->x_.setTimeScale(impl_->timeScale_);
  impl_->y_.setTimeScale(impl_->timeScale_);
  impl_->z_.setTimeScale(impl_->timeScale_);
  impl_->rotation_.setTimeScale(impl_->timeScale_);
  impl_->rotationX_.setTimeScale(impl_->timeScale_);
  impl_->rotationY_.setTimeScale(impl_->timeScale_);
  impl_->scaleX_.setTimeScale(impl_->timeScale_);
  impl_->scaleY_.setTimeScale(impl_->timeScale_);
  impl_->scaleZ_.setTimeScale(impl_->timeScale_);
  impl_->anchorX_.setTimeScale(impl_->timeScale_);
  impl_->anchorY_.setTimeScale(impl_->timeScale_);
  impl_->anchorZ_.setTimeScale(impl_->timeScale_);

 }

 SharedPtr<AbstractProperty> AnimatableTransform3D::channelProperty(TransformChannel channel) const {
   switch (channel) {
   case TransformChannel::PositionX: return impl_->x_.property();
   case TransformChannel::PositionY: return impl_->y_.property();
   case TransformChannel::PositionZ: return impl_->z_.property();
   case TransformChannel::Rotation: return impl_->rotation_.property();
   case TransformChannel::RotationX: return impl_->rotationX_.property();
   case TransformChannel::RotationY: return impl_->rotationY_.property();
   case TransformChannel::ScaleX: return impl_->scaleX_.property();
   case TransformChannel::ScaleY: return impl_->scaleY_.property();
   case TransformChannel::ScaleZ: return impl_->scaleZ_.property();
   case TransformChannel::AnchorX: return impl_->anchorX_.property();
   case TransformChannel::AnchorY: return impl_->anchorY_.property();
   case TransformChannel::AnchorZ: return impl_->anchorZ_.property();
   }
   return {};
 }

 int64_t AnimatableTransform3D::keyframeTimeScale() const
 {
  return impl_->timeScale_;
 }

void AnimatableTransform3D::setInitalAngle(const RationalTime& time, float angle/*=0*/)
{
  impl_->rotation_.setOffset(angle);
  if (impl_->rotation_.getKeyFrameCount() == 0) impl_->rotation_.setCurrent(0.0f);
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

void AnimatableTransform3D::setInitialRotation(const RationalTime& time, float angle)
{
  impl_->rotation_.setOffset(angle);
  if (impl_->rotation_.getKeyFrameCount() == 0) impl_->rotation_.setCurrent(0.0f);
  impl_->initialRotation_ = angle;
  impl_->currentRotation_ = angle;
}

void AnimatableTransform3D::setAutoOrient(bool enabled)
{
  impl_->autoOrientMode_ = enabled ? AutoOrientMode::AlongPath : AutoOrientMode::Off;
}

bool AnimatableTransform3D::isAutoOrient() const
{
  return impl_->autoOrientMode_ != AutoOrientMode::Off;
}

void AnimatableTransform3D::setAutoOrientMode(AutoOrientMode mode)
{
  impl_->autoOrientMode_ = mode;
}

AutoOrientMode AnimatableTransform3D::autoOrientMode() const
{
  return impl_->autoOrientMode_;
}

void AnimatableTransform3D::setInitialPosition(const RationalTime& time, float px, float py)
{
  impl_->x_.setOffset(px);
  if (impl_->x_.getKeyFrameCount() == 0) impl_->x_.setCurrent(0.0f);
  impl_->initialX_ = px;
  impl_->y_.setOffset(py);
  if (impl_->y_.getKeyFrameCount() == 0) impl_->y_.setCurrent(0.0f);
  impl_->initialY_ = py;
  impl_->currentX_ = px;
  impl_->currentY_ = py;
}

void AnimatableTransform3D::setCurrentPosition(float x, float y)
{
  impl_->x_.setCurrent(x - impl_->initialX_);
  impl_->y_.setCurrent(y - impl_->initialY_);

  impl_->currentX_ = x;
  impl_->currentY_ = y;
}

void AnimatableTransform3D::setCurrentPositionZ(float z)
{
  impl_->z_.setCurrent(z);

  impl_->currentZ_ = z;
}

void AnimatableTransform3D::setCurrentRotation(float degrees)
{
  impl_->rotation_.setCurrent(degrees - impl_->initialRotation_);

  impl_->currentRotation_ = degrees;
}

void AnimatableTransform3D::setCurrentRotationX(float degrees)
{
  impl_->rotationX_.setCurrent(degrees);

  impl_->currentRotationX_ = degrees;
}

void AnimatableTransform3D::setCurrentRotationY(float degrees)
{
  impl_->rotationY_.setCurrent(degrees);

  impl_->currentRotationY_ = degrees;
}

void AnimatableTransform3D::setCurrentRotationZ(float degrees)
{
  setCurrentRotation(degrees);
}

void AnimatableTransform3D::setCurrentScale(float xs, float ys)
{
  impl_->scaleX_.setCurrent(xs);
  impl_->scaleY_.setCurrent(ys);

  impl_->currentScaleX_ = xs;
  impl_->currentScaleY_ = ys;
}

void AnimatableTransform3D::setCurrentAnchor(float x, float y, float z)
{
  impl_->anchorX_.setCurrent(x);
  impl_->anchorY_.setCurrent(y);
  impl_->anchorZ_.setCurrent(z);
}

void AnimatableTransform3D::setPosition(const RationalTime& time, float x, float y)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->x_.addKeyFrame(frame, x);
  impl_->y_.addKeyFrame(frame, y);
  impl_->currentX_ = impl_->initialX_ + x;
  impl_->currentY_ = impl_->initialY_ + y;
}

void AnimatableTransform3D::setPositionZ(const RationalTime& time, float z)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->z_.addKeyFrame(frame, z);
  impl_->currentZ_ = z;
}

void AnimatableTransform3D::setAnchor(const RationalTime& time, float x, float y, float z)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->anchorX_.addKeyFrame(frame, x);
  impl_->anchorY_.addKeyFrame(frame, y);
  impl_->anchorZ_.addKeyFrame(frame, z);
}

void AnimatableTransform3D::setRotation(const RationalTime& time, float degrees)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->rotation_.addKeyFrame(frame, degrees);
  impl_->currentRotation_ = degrees;
}

void AnimatableTransform3D::setRotationX(const RationalTime& time, float degrees)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->rotationX_.addKeyFrame(frame, degrees);
  impl_->currentRotationX_ = degrees;
}

void AnimatableTransform3D::setRotationY(const RationalTime& time, float degrees)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->rotationY_.addKeyFrame(frame, degrees);
  impl_->currentRotationY_ = degrees;
}

void AnimatableTransform3D::setRotationZ(const RationalTime& time, float degrees)
{
  setRotation(time, degrees);
}

float AnimatableTransform3D::positionX() const
{
  return impl_->x_.getKeyFrameCount() == 0
      ? impl_->initialX_ + impl_->x_.current() : impl_->currentX_;
}

float AnimatableTransform3D::positionY() const
{
  return impl_->y_.getKeyFrameCount() == 0
      ? impl_->initialY_ + impl_->y_.current() : impl_->currentY_;
}

float AnimatableTransform3D::positionZ() const
{
  return impl_->z_.getKeyFrameCount() == 0
      ? impl_->z_.current() : impl_->currentZ_;
}

float AnimatableTransform3D::rotation() const
{
  return impl_->rotation_.getKeyFrameCount() == 0
      ? impl_->initialRotation_ + impl_->rotation_.current() : impl_->currentRotation_;
}

float AnimatableTransform3D::rotationX() const
{
  return impl_->rotationX_.getKeyFrameCount() == 0
      ? impl_->rotationX_.current() : impl_->currentRotationX_;
}

float AnimatableTransform3D::rotationY() const
{
  return impl_->rotationY_.getKeyFrameCount() == 0
      ? impl_->rotationY_.current() : impl_->currentRotationY_;
}

float AnimatableTransform3D::rotationZ() const
{
  return rotation();
}

float AnimatableTransform3D::initialRotation() const
{
  return impl_->initialRotation_;
}

float AnimatableTransform3D::scaleX() const
{
  return impl_->scaleX_.getKeyFrameCount() == 0
      ? impl_->scaleX_.current() : impl_->currentScaleX_;
}

float AnimatableTransform3D::scaleY() const
{
  return impl_->scaleY_.getKeyFrameCount() == 0
      ? impl_->scaleY_.current() : impl_->currentScaleY_;
}

float AnimatableTransform3D::scaleZ() const
{
  return impl_->scaleZ_.getKeyFrameCount() == 0
      ? impl_->scaleZ_.current() : impl_->currentScaleZ_;
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
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->scaleX_.addKeyFrame(frame, xs);
  impl_->scaleY_.addKeyFrame(frame, ys);
  impl_->currentScaleX_ = xs;
  impl_->currentScaleY_ = ys;
}

void AnimatableTransform3D::setScale(const RationalTime& time, float xs, float ys, float zs)
{
  setScale(time, xs, ys);
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->scaleZ_.addKeyFrame(frame, zs);
  impl_->currentScaleZ_ = zs;
}

// ============================================
// �V�K���\�b�h�F�����w��̃A�j���[�V������Ԓl�擾
// ============================================
float AnimatableTransform3D::positionXAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(impl_->timeScale_));
  if (const auto spatial = impl_->spatialPositionAt(frame)) {
    return spatial->x;
  }
  return impl_->x_.at(frame);
}

float AnimatableTransform3D::positionYAt(const RationalTime& time) const
{
  FramePosition frame(time.rescaledTo(impl_->timeScale_));
  if (const auto spatial = impl_->spatialPositionAt(frame)) {
    return spatial->y;
  }
  return impl_->y_.at(frame);
}

float AnimatableTransform3D::positionZAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->z_.at(frame);
}

float AnimatableTransform3D::rotationAt(const RationalTime& time) const
{
  if (impl_->autoOrientMode_ != AutoOrientMode::Off) {
    const auto posTimes = getPositionKeyFrameTimes();
    if (posTimes.size() < 2) {
      return impl_->currentRotation_;
    }

    FramePosition targetFrame(time.toFrameCount(impl_->timeScale_));
    int64_t target = targetFrame.framePosition();

    int64_t prevFrame = posTimes.front().toFrameCount(impl_->timeScale_);
    int64_t nextFrame = posTimes.back().toFrameCount(impl_->timeScale_);
    if (impl_->autoOrientMode_ == AutoOrientMode::AlongPathAtFrameStart) {
      prevFrame = posTimes.front().toFrameCount(impl_->timeScale_);
      nextFrame = posTimes.size() > 1 ? posTimes[1].toFrameCount(impl_->timeScale_) : prevFrame;
    } else {
      for (const auto& pt : posTimes) {
        int64_t f = pt.toFrameCount(impl_->timeScale_);
        if (f <= target) prevFrame = f;
        if (f >= target && nextFrame >= target) {
          nextFrame = f;
          break;
        }
      }
      if (nextFrame < target) nextFrame = target;
    }

    const QPointF prev(impl_->x_.at(FramePosition(prevFrame)),
                       impl_->y_.at(FramePosition(prevFrame)));
    const QPointF next(impl_->x_.at(FramePosition(nextFrame)),
                       impl_->y_.at(FramePosition(nextFrame)));
    const float dx = next.x() - prev.x();
    const float dy = next.y() - prev.y();
    if (std::abs(dx) < 1e-4f && std::abs(dy) < 1e-4f) {
      return impl_->currentRotation_;
    }
    float angle = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;
    return angle;
  }

  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->rotation_.at(frame);
}

float AnimatableTransform3D::rotationXAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->rotationX_.at(frame);
}

float AnimatableTransform3D::rotationYAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->rotationY_.at(frame);
}

float AnimatableTransform3D::rotationZAt(const RationalTime& time) const
{
  return rotationAt(time);
}

float AnimatableTransform3D::scaleXAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->scaleX_.at(frame);
}

float AnimatableTransform3D::scaleYAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->scaleY_.at(frame);
}

float AnimatableTransform3D::anchorXAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->anchorX_.at(frame);
}

float AnimatableTransform3D::anchorYAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->anchorY_.at(frame);
}

float AnimatableTransform3D::anchorZAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
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
  float4x4 scaleMatrix = float4x4::Scale(scaleX(), scaleY(), scaleZ());
  
  // 2. Euler rotation in Z * Y * X order. Z remains the legacy rotation.
  float4x4 rotationMatrix = makeEulerRotation(
      rotationX(), rotationY(),
      rotation());
  
  // 3. sړs
  float4x4 translationMatrix = float4x4::Translation(
      positionX(), positionY(), positionZ());
  
  // 4. sFTranslation * Rotation * Scale
  matrix = translationMatrix * rotationMatrix * scaleMatrix;
  
  return matrix;
}

float4x4 AnimatableTransform3D::getAllMatrix() const
{
  float4x4 scaleMatrix = float4x4::Scale(
      scaleX(), scaleY(), scaleZ());

  float4x4 rotationMatrix = makeEulerRotation(
      rotationX(), rotationY(),
      rotation());

  float ax = impl_->anchorX_.current();
  float ay = impl_->anchorY_.current();
  float az = impl_->anchorZ_.current();

  float4x4 anchorMatrix = float4x4::Translation(-ax, -ay, -az);
  float4x4 translationMatrix = float4x4::Translation(positionX(), positionY(), positionZ());

  return translationMatrix * rotationMatrix * scaleMatrix * anchorMatrix;
}

// ============================================
// 4x4�ϊ��s��̐����i�w�莞���̃A�j���[�V�����l����j
// ============================================
float4x4 AnimatableTransform3D::getMatrixAt(const RationalTime& time) const
{
  // �w�莞���̒l���擾
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  
  float px = impl_->x_.at(frame);
  float py = impl_->y_.at(frame);
  float rot = impl_->rotation_.at(frame);
  float rotX = rotationXAt(time);
  float rotY = rotationYAt(time);
  float sx = impl_->scaleX_.at(frame);
  float sy = impl_->scaleY_.at(frame);
  
  // �ϊ������FScale �� Rotation �� Translation
  float4x4 matrix = float4x4::Identity();
  
  // 1. �X�P�[���s��
  float4x4 scaleMatrix = float4x4::Scale(sx, sy, impl_->scaleZ_.at(frame));
  
  // 2. Euler rotation in Z * Y * X order.
  float4x4 rotationMatrix = makeEulerRotation(rotX, rotY, rot);
  
  // 3. s�r�s��
  float4x4 translationMatrix =
      float4x4::Translation(px, py, impl_->z_.at(frame));
  
  // 4. s����Translation * Rotation * Scale
  matrix = translationMatrix * rotationMatrix * scaleMatrix;
  
  return matrix;
}

float4x4 AnimatableTransform3D::getAllMatrixAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));

  // 1. Get Offset values (Animation Keyframes)
  float oz = impl_->z_.at(frame);
  float orot = impl_->rotation_.at(frame);
  float orotX = rotationXAt(time);
  float orotY = rotationYAt(time);
  float osx = impl_->scaleX_.at(frame);
  float osy = impl_->scaleY_.at(frame);
  float osz = impl_->scaleZ_.at(frame);
  float ax = impl_->anchorX_.at(frame);
  float ay = impl_->anchorY_.at(frame);
  float az = impl_->anchorZ_.at(frame);

  // 2. Combine Baseline (Layout) + Offset (Animation)
  float finalX = impl_->initialX_ + positionXAt(time);
  float finalY = impl_->initialY_ + positionYAt(time);
  float finalZ = impl_->initialZ_ + oz;
  float finalRot = impl_->initialRotation_ + orot;
  float finalScaleX = osx; // Multiplicative scale
  float finalScaleY = osy;
  float finalScaleZ = osz;

  // 3. Construct Final Matrix (T * R * S * A)
  float4x4 scaleMatrix = float4x4::Scale(finalScaleX, finalScaleY, finalScaleZ);

  float4x4 rotationMatrix = makeEulerRotation(orotX, orotY, finalRot);

  float4x4 anchorMatrix = float4x4::Translation(-ax, -ay, -az);
  float4x4 translationMatrix = float4x4::Translation(finalX, finalY, finalZ);

  return translationMatrix * rotationMatrix * scaleMatrix * anchorMatrix;
}

Transform3DSnapshot AnimatableTransform3D::snapshot() const
{
  Transform3DSnapshot snapshot;
  snapshot.positionX = positionX();
  snapshot.positionY = positionY();
  snapshot.positionZ = positionZ();
  snapshot.rotation = rotation();
  snapshot.rotationX = rotationX();
  snapshot.rotationY = rotationY();
  snapshot.rotationZ = rotation();
  snapshot.scaleX = scaleX();
  snapshot.scaleY = scaleY();
  snapshot.scaleZ = scaleZ();
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
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  snapshot.positionX = impl_->initialX_ + positionXAt(time);
  snapshot.positionY = impl_->initialY_ + positionYAt(time);
  snapshot.positionZ = impl_->initialZ_ + impl_->z_.at(frame);
  snapshot.rotation = impl_->initialRotation_ + impl_->rotation_.at(frame);
  snapshot.rotationX = rotationXAt(time);
  snapshot.rotationY = rotationYAt(time);
  snapshot.rotationZ = snapshot.rotation;
  snapshot.scaleX = impl_->scaleX_.at(frame);
  snapshot.scaleY = impl_->scaleY_.at(frame);
  snapshot.scaleZ = impl_->scaleZ_.at(frame);
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
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->x_.hasKeyFrameAt(frame) || impl_->y_.hasKeyFrameAt(frame);
}

bool AnimatableTransform3D::setPositionKeyFrameValueAt(const RationalTime& time,
                                                       float x, float y)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
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
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  const bool xOk = impl_->x_.setKeyFrameInterpolationAt(frame, xInterpolation);
  const bool yOk = impl_->y_.setKeyFrameInterpolationAt(frame, yInterpolation);
  return xOk || yOk;
}

bool AnimatableTransform3D::setPositionKeyFrameSpatialTangentsAt(
    const RationalTime& time, const PositionSpatialTangents& tangents)
{
  const FramePosition frame(time.rescaledTo(impl_->timeScale_));
  if (!impl_->x_.hasKeyFrameAt(frame) && !impl_->y_.hasKeyFrameAt(frame)) {
    return false;
  }
  impl_->positionSpatialTangents_[frame.framePosition()] = tangents;
  return true;
}

bool AnimatableTransform3D::positionKeyFrameSpatialTangentsAt(
    const RationalTime& time, PositionSpatialTangents& tangents) const
{
  const FramePosition frame(time.rescaledTo(impl_->timeScale_));
  const auto it = impl_->positionSpatialTangents_.find(frame.framePosition());
  if (it == impl_->positionSpatialTangents_.end()) {
    return false;
  }
  tangents = it->second;
  return true;
}

bool AnimatableTransform3D::hasPositionSpatialTangents() const
{
  if (impl_->x_.getKeyFrameCount() < 2 || impl_->y_.getKeyFrameCount() < 2) return false;
  for (const auto& [frame, tangent] : impl_->positionSpatialTangents_)
    if (impl_->x_.hasKeyFrameAt(FramePosition(frame)) &&
        impl_->y_.hasKeyFrameAt(FramePosition(frame))) return true;
  return false;
}

bool AnimatableTransform3D::removePositionKeyFrameSpatialTangentsAt(
    const RationalTime& time)
{
  const FramePosition frame(time.rescaledTo(impl_->timeScale_));
  return impl_->positionSpatialTangents_.erase(frame.framePosition()) > 0;
}

InterpolationType AnimatableTransform3D::positionXKeyFrameInterpolationAt(
    const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->x_.getKeyFrameInterpolationAt(frame);
}

InterpolationType AnimatableTransform3D::positionYKeyFrameInterpolationAt(
    const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->y_.getKeyFrameInterpolationAt(frame);
}

void AnimatableTransform3D::removePositionKeyFrameAt(const RationalTime& time)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->x_.removeKeyFrameAt(frame);
  impl_->y_.removeKeyFrameAt(frame);
  impl_->positionSpatialTangents_.erase(frame.framePosition());
}

void AnimatableTransform3D::clearPositionKeyFrames()
{
  impl_->x_.clearKeyFrames();
  impl_->y_.clearKeyFrames();
  impl_->positionSpatialTangents_.clear();
}

size_t AnimatableTransform3D::getPositionKeyFrameCount() const
{
  // X �� Y �̃L�[�t���[�����̍ő�l��Ԃ��i�ʏ�͓����͂��j
  return std::max(impl_->x_.getKeyFrameCount(), impl_->y_.getKeyFrameCount());
}

std::vector<RationalTime> AnimatableTransform3D::getPositionKeyFrameTimes() const
{
  return collectUniqueKeyFrameTimes(impl_->timeScale_, impl_->x_, impl_->y_);
}

// Rotation �L�[�t���[���Ǘ�
bool AnimatableTransform3D::hasRotationKeyFrameAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->rotation_.hasKeyFrameAt(frame) ||
         impl_->rotationX_.hasKeyFrameAt(frame) ||
         impl_->rotationY_.hasKeyFrameAt(frame);
}

void AnimatableTransform3D::removeRotationKeyFrameAt(const RationalTime& time)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  impl_->rotation_.removeKeyFrameAt(frame);
  impl_->rotationX_.removeKeyFrameAt(frame);
  impl_->rotationY_.removeKeyFrameAt(frame);
}

void AnimatableTransform3D::clearRotationKeyFrames()
{
  impl_->rotation_.clearKeyFrames();
  impl_->rotationX_.clearKeyFrames();
  impl_->rotationY_.clearKeyFrames();
}

size_t AnimatableTransform3D::getRotationKeyFrameCount() const
{
  return std::max({impl_->rotation_.getKeyFrameCount(),
                   impl_->rotationX_.getKeyFrameCount(),
                   impl_->rotationY_.getKeyFrameCount()});
}

std::vector<RationalTime> AnimatableTransform3D::getRotationKeyFrameTimes() const
{
  return collectUniqueKeyFrameTimes(impl_->timeScale_, impl_->rotation_, impl_->rotationX_,
                                    impl_->rotationY_);
}

// Scale �L�[�t���[���Ǘ�
bool AnimatableTransform3D::hasScaleKeyFrameAt(const RationalTime& time) const
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
  return impl_->scaleX_.hasKeyFrameAt(frame) || impl_->scaleY_.hasKeyFrameAt(frame);
}

void AnimatableTransform3D::removeScaleKeyFrameAt(const RationalTime& time)
{
  FramePosition frame(time.toFrameCount(impl_->timeScale_));
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
  return collectUniqueKeyFrameTimes(impl_->timeScale_, impl_->scaleX_, impl_->scaleY_);
}

std::vector<RationalTime> AnimatableTransform3D::getAllKeyFrameTimes() const
{
  return collectUniqueKeyFrameTimes(impl_->timeScale_, impl_->x_, impl_->y_, impl_->z_,
                                    impl_->rotation_, impl_->rotationX_,
                                    impl_->rotationY_, impl_->scaleX_,
                                    impl_->scaleY_, impl_->anchorX_,
                                    impl_->anchorY_, impl_->anchorZ_);
}

// ���ׂăN���A
void AnimatableTransform3D::clearAllKeyFrames()
{
  clearPositionKeyFrames();
  clearRotationKeyFrames();
  clearScaleKeyFrames();
  impl_->z_.clearKeyFrames();
  impl_->scaleZ_.clearKeyFrames();
  impl_->anchorX_.clearKeyFrames();
  impl_->anchorY_.clearKeyFrames();
  impl_->anchorZ_.clearKeyFrames();
}

}
