module;
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module Core.Camera;

import Float3;

export namespace ArtifactCore
{

 /// 3D DCC-style camera with orbit, pan, zoom, and projection.
 class Camera {
 public:
  Camera();
  ~Camera();

  // --- View setup ---

  void lookAt(const float3<float>& eye,
              const float3<float>& target,
              const float3<float>& up = { 0, 1, 0 });

  // --- Orbit controls (spherical coordinates around target) ---

  void orbit(float deltaYaw, float deltaPitch);
  float yaw() const;
  float yawRadians() const;
  void setYaw(float degrees);
  float pitch() const;
  float pitchRadians() const;
  void setPitch(float degrees);
  float distance() const;
  void setDistance(float dist);

  // --- Pan ---

  void pan(float dx, float dy);

  // --- Zoom (dolly) ---

  void dolly(float delta);

  // --- Projection ---

  void setPerspective(float fovYDegrees, float aspect, float nearZ, float farZ);
  float fovY() const { return fovY_; }
  float aspect() const { return aspect_; }
  float nearZ() const { return nearZ_; }
  float farZ() const { return farZ_; }
  void setFovY(float degrees);
  void setAspect(float ratio);

  // --- Accessors ---

  float3<float> position() const { return position_; }
  float3<float> target() const { return target_; }
  float3<float> up() const { return up_; }
  float3<float> forward() const;
  float3<float> right() const;

  // --- Matrices ---

  glm::mat4 viewMatrix() const;
  glm::mat4 projectionMatrix() const;
  glm::mat4 viewProjectionMatrix() const;

  // --- Presets ---

  void reset();
  void frameAll(float boundingRadius = 10.0f);
  void setViewFront();
  void setViewBack();
  void setViewLeft();
  void setViewRight();
  void setViewTop();
  void setViewBottom();

  // --- Fit ---

  void fitToSphere(const float3<float>& center, float radius, float margin = 1.2f);

 private:
  void updateFromOrbit();

  // View state
  float3<float> position_ = { 0, 0, 5 };
  float3<float> target_   = { 0, 0, 0 };
  float3<float> up_       = { 0, 1, 0 };

  // Orbit angles (degrees)
  float yaw_   = 0.0f;
  float pitch_ = 0.0f;
  float distance_ = 5.0f;

  // Projection
  float fovY_  = 45.0f;
  float aspect_ = 16.0f / 9.0f;
  float nearZ_ = 0.1f;
  float farZ_  = 1000.0f;
 };

};
