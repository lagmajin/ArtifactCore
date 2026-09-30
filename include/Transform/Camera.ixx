module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <QMatrix4x4>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module Core.Camera;

import Float3;
import Math.Vec;

export namespace ArtifactCore
{

 enum class StereoMode : int {
  Mono = 0,
  TopBottom = 1,
  SideBySide = 2,
 };

 /// 3D DCC-style camera with orbit, pan, zoom, and projection.
 class Camera {
 public:
  Camera();
  ~Camera();

  // --- View setup ---

  void lookAt(Coordinates::WorldPoint3 eye,
              Coordinates::WorldPoint3 target,
              Coordinates::WorldVector3 up = { 0, 1, 0 });

  // --- Orbit controls (spherical coordinates around target) ---

  void orbit(Units::Degrees deltaYaw, Units::Degrees deltaPitch);
  Units::Degrees yaw() const;
  Units::Radians yawRadians() const;
  void setYaw(Units::Degrees degrees);
  Units::Degrees pitch() const;
  Units::Radians pitchRadians() const;
  void setPitch(Units::Degrees degrees);
  Units::WorldLength distance() const;
  void setDistance(Units::WorldLength dist);

  // --- Pan ---

  void pan(Units::WorldLength dx, Units::WorldLength dy);

  // --- Zoom (dolly) ---

  void dolly(Units::WorldLength delta);

  // --- Projection ---

  void setPerspective(Units::Degrees fovY,
                      float aspect,
                      Units::Pixels nearZ,
                      Units::Pixels farZ);
  Units::Degrees fovY() const { return fovY_; }
  float aspect() const { return aspect_; }
  Units::Pixels nearZ() const { return nearZ_; }
  Units::Pixels farZ() const { return farZ_; }
  void setFovY(Units::Degrees degrees);
  void setAspect(float ratio);

  // --- Accessors ---

  Coordinates::WorldPoint3 position() const { return position_; }
  Coordinates::WorldPoint3 target() const { return target_; }
  Coordinates::WorldVector3 up() const { return up_; }
  Coordinates::WorldVector3 forward() const;
  Coordinates::WorldVector3 right() const;

  // --- Matrices ---

  glm::mat4 viewMatrix() const;
  glm::mat4 projectionMatrix() const;
  glm::mat4 viewProjectionMatrix() const;

  // --- Presets ---

  void reset();
  void frameAll(Units::WorldLength boundingRadius = Units::WorldLength{10.0f});
  void setViewFront();
  void setViewBack();
  void setViewLeft();
  void setViewRight();
  void setViewTop();
  void setViewBottom();

  // --- Fit ---

  void fitToSphere(Coordinates::WorldPoint3 center,
                   Units::WorldLength radius,
                   float margin = 1.2f);

 private:
  void updateFromOrbit();

  // View state
  Coordinates::WorldPoint3 position_ = { 0, 0, 5 };
  Coordinates::WorldPoint3 target_   = { 0, 0, 0 };
  Coordinates::WorldVector3 up_      = { 0, 1, 0 };

  // Orbit angles (degrees)
  Units::Degrees yaw_{};
  Units::Degrees pitch_{};
  Units::WorldLength distance_{5.0f};

  // Projection
  Units::Degrees fovY_{45.0f};
  float aspect_ = 16.0f / 9.0f;
  Units::Pixels nearZ_{0.1f};
  Units::Pixels farZ_{1000.0f};
 };

 struct StereoCamera {
  QMatrix4x4 leftEyeView;
  QMatrix4x4 rightEyeView;
  QMatrix4x4 projection;
  Units::Meters ipd{0.064f};
  Units::Pixels nearPlane{0.1f};
  Units::Pixels farPlane{1000.0f};

  static StereoCamera fromHmd(const QMatrix4x4& hmdPose,
                              Units::Meters ipdValue = Units::Meters{0.064f},
                              Units::Pixels nearValue = Units::Pixels{0.1f},
                              Units::Pixels farValue = Units::Pixels{1000.0f})
  {
   StereoCamera camera;
   camera.ipd = ipdValue;
   camera.nearPlane = nearValue;
   camera.farPlane = farValue;

   QMatrix4x4 eyeOffset;
   eyeOffset.translate(ipdValue.value * 0.5f, 0.0f, 0.0f);
   camera.leftEyeView = (hmdPose * eyeOffset).inverted();
   camera.rightEyeView = (hmdPose * eyeOffset.inverted()).inverted();
   return camera;
  }
 };

};
