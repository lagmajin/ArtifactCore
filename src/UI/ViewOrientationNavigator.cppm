module;
#include <utility>

#include <QtGui/QMatrix4x4>
#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>

#include <algorithm>
#include <cmath>

module UI.View.Orientation.Navigator;

namespace ArtifactCore
{
 namespace
 {
  constexpr float kMinSnapDuration = 0.01f;
  constexpr float kMaxSnapDuration = 2.0f;
  constexpr float kSqrtHalf = 0.70710678f;

  QQuaternion lookRotation(const QVector3D& forward, const QVector3D& upHint = QVector3D(0.0f, 1.0f, 0.0f))
  {
   QVector3D z = -forward.normalized();
   QVector3D x = QVector3D::crossProduct(upHint, z).normalized();
   if (x.lengthSquared() < 1.0e-8f)
   {
    x = QVector3D(1.0f, 0.0f, 0.0f);
   }
   QVector3D y = QVector3D::crossProduct(z, x).normalized();

   QMatrix4x4 basis(
    x.x(), y.x(), z.x(), 0.0f,
    x.y(), y.y(), z.y(), 0.0f,
    x.z(), y.z(), z.z(), 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
   return QQuaternion::fromRotationMatrix(basis.toGenericMatrix<3, 3>()).normalized();
  }
 }

 ViewOrientationNavigator::ViewOrientationNavigator()
  : current_(QQuaternion()),
  start_(QQuaternion()),
  target_(QQuaternion())
 {
 }

 void ViewOrientationNavigator::setCurrentOrientation(const QQuaternion& orientation)
 {
  current_ = orientation.normalized();
  if (!animating_)
  {
   start_ = current_;
   target_ = current_;
  }
 }

 QQuaternion ViewOrientationNavigator::currentOrientation() const
 {
  return current_;
 }

 void ViewOrientationNavigator::setSnapDurationSeconds(const float seconds)
 {
  snapDurationSeconds_ = std::clamp(seconds, kMinSnapDuration, kMaxSnapDuration);
 }

 float ViewOrientationNavigator::snapDurationSeconds() const
 {
  return snapDurationSeconds_;
 }

 void ViewOrientationNavigator::snapTo(const ViewOrientationHotspot hotspot, const bool immediate)
 {
  hotspot_ = hotspot;
  target_ = orientationForHotspot(hotspot);
  if (immediate)
  {
   current_ = target_;
   start_ = target_;
   elapsedSeconds_ = 0.0f;
   animating_ = false;
   return;
  }

  start_ = current_;
  elapsedSeconds_ = 0.0f;
  animating_ = true;
 }

 bool ViewOrientationNavigator::isAnimating() const
 {
  return animating_;
 }

 ViewOrientationHotspot ViewOrientationNavigator::activeHotspot() const
 {
  return hotspot_;
 }

 QQuaternion ViewOrientationNavigator::targetOrientation() const
 {
  return target_;
 }

 bool ViewOrientationNavigator::update(const float deltaSeconds)
 {
  if (!animating_)
  {
   return false;
  }

  elapsedSeconds_ += std::max(0.0f, deltaSeconds);
  const float t = std::clamp(elapsedSeconds_ / std::max(snapDurationSeconds_, kMinSnapDuration), 0.0f, 1.0f);
  current_ = QQuaternion::slerp(start_, target_, t).normalized();
  if (t >= 1.0f)
  {
   current_ = target_;
   animating_ = false;
  }
  return true;
 }

 ViewOrientationHotspot ViewOrientationNavigator::resolveHotspotFromDirection(
  const QVector3D& localDirection,
  const float faceThreshold)
 {
  const QVector3D d = localDirection.normalized();
  if (d.lengthSquared() < 1.0e-8f)
  {
   return ViewOrientationHotspot::None;
  }

  const float ax = std::abs(d.x());
  const float ay = std::abs(d.y());
  const float az = std::abs(d.z());

  if (az >= faceThreshold && ax < kSqrtHalf && ay < kSqrtHalf)
  {
   return d.z() >= 0.0f ? ViewOrientationHotspot::Front : ViewOrientationHotspot::Back;
  }
  if (ax >= faceThreshold && ay < kSqrtHalf && az < kSqrtHalf)
  {
   return d.x() >= 0.0f ? ViewOrientationHotspot::Right : ViewOrientationHotspot::Left;
  }
  if (ay >= faceThreshold && ax < kSqrtHalf && az < kSqrtHalf)
  {
   return d.y() >= 0.0f ? ViewOrientationHotspot::Top : ViewOrientationHotspot::Bottom;
  }

  if (d.z() >= 0.0f)
  {
   if (d.y() >= 0.0f && az >= ax) return ViewOrientationHotspot::FrontTop;
   if (d.y() < 0.0f && az >= ax) return ViewOrientationHotspot::FrontBottom;
   if (d.x() >= 0.0f) return ViewOrientationHotspot::FrontRight;
   return ViewOrientationHotspot::FrontLeft;
  }

  if (d.y() >= 0.0f && az >= ax) return ViewOrientationHotspot::BackTop;
  if (d.y() < 0.0f && az >= ax) return ViewOrientationHotspot::BackBottom;
  if (d.x() >= 0.0f) return ViewOrientationHotspot::BackRight;
  return ViewOrientationHotspot::BackLeft;
 }

 QQuaternion ViewOrientationNavigator::orientationForHotspot(const ViewOrientationHotspot hotspot)
 {
  switch (hotspot)
  {
  case ViewOrientationHotspot::Front: return lookRotation(QVector3D(0.0f, 0.0f, -1.0f));
  case ViewOrientationHotspot::Back: return lookRotation(QVector3D(0.0f, 0.0f, 1.0f));
  case ViewOrientationHotspot::Left: return lookRotation(QVector3D(-1.0f, 0.0f, 0.0f));
  case ViewOrientationHotspot::Right: return lookRotation(QVector3D(1.0f, 0.0f, 0.0f));
  case ViewOrientationHotspot::Top: return lookRotation(QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, -1.0f));
  case ViewOrientationHotspot::Bottom: return lookRotation(QVector3D(0.0f, -1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f));

  case ViewOrientationHotspot::FrontTop: return lookRotation(QVector3D(0.0f, 1.0f, -1.0f));
  case ViewOrientationHotspot::FrontBottom: return lookRotation(QVector3D(0.0f, -1.0f, -1.0f));
  case ViewOrientationHotspot::FrontLeft: return lookRotation(QVector3D(-1.0f, 0.0f, -1.0f));
  case ViewOrientationHotspot::FrontRight: return lookRotation(QVector3D(1.0f, 0.0f, -1.0f));
  case ViewOrientationHotspot::BackTop: return lookRotation(QVector3D(0.0f, 1.0f, 1.0f));
  case ViewOrientationHotspot::BackBottom: return lookRotation(QVector3D(0.0f, -1.0f, 1.0f));
  case ViewOrientationHotspot::BackLeft: return lookRotation(QVector3D(-1.0f, 0.0f, 1.0f));
  case ViewOrientationHotspot::BackRight: return lookRotation(QVector3D(1.0f, 0.0f, 1.0f));
  case ViewOrientationHotspot::LeftTop: return lookRotation(QVector3D(-1.0f, 1.0f, 0.0f));
  case ViewOrientationHotspot::LeftBottom: return lookRotation(QVector3D(-1.0f, -1.0f, 0.0f));
  case ViewOrientationHotspot::RightTop: return lookRotation(QVector3D(1.0f, 1.0f, 0.0f));
  case ViewOrientationHotspot::RightBottom: return lookRotation(QVector3D(1.0f, -1.0f, 0.0f));

  case ViewOrientationHotspot::None:
  default:
   return QQuaternion();
  }
 }
}
