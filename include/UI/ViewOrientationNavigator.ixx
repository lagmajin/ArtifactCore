module;
#include <utility>
#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>

export module UI.View.Orientation.Navigator;

export namespace ArtifactCore
{
 enum class ViewOrientationHotspot
 {
  None = 0,
  Front,
  Back,
  Left,
  Right,
  Top,
  Bottom,
  FrontTop,
  FrontBottom,
  FrontLeft,
  FrontRight,
  BackTop,
  BackBottom,
  BackLeft,
  BackRight,
  LeftTop,
  LeftBottom,
  RightTop,
  RightBottom
 };

 class ViewOrientationNavigator
 {
 public:
  ViewOrientationNavigator();

  void setCurrentOrientation(const QQuaternion& orientation);
  [[nodiscard]] QQuaternion currentOrientation() const;

  void setSnapDurationSeconds(float seconds);
  [[nodiscard]] float snapDurationSeconds() const;

  void snapTo(ViewOrientationHotspot hotspot, bool immediate = false);
  [[nodiscard]] bool isAnimating() const;
  [[nodiscard]] ViewOrientationHotspot activeHotspot() const;
  [[nodiscard]] QQuaternion targetOrientation() const;

  // Returns true while orientation changed by animation.
  bool update(float deltaSeconds);

  // Renderer/interaction helper: resolve cube click direction to hotspot.
  [[nodiscard]] static ViewOrientationHotspot resolveHotspotFromDirection(
   const QVector3D& localDirection,
   float faceThreshold = 0.86f);

  [[nodiscard]] static QQuaternion orientationForHotspot(ViewOrientationHotspot hotspot);

 private:
  QQuaternion current_;
  QQuaternion start_;
  QQuaternion target_;
  float snapDurationSeconds_ = 0.18f;
  float elapsedSeconds_ = 0.0f;
  bool animating_ = false;
  ViewOrientationHotspot hotspot_ = ViewOrientationHotspot::None;
 };
}
