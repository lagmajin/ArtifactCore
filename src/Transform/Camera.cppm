module;
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

module Core.Camera;

import Float3;

namespace ArtifactCore
{

namespace {
 constexpr float kPi = 3.14159265358979323846f;
 constexpr float kDeg2Rad = kPi / 180.0f;
 constexpr float kRad2Deg = 180.0f / kPi;
 constexpr float kMinDistance = 0.01f;
 constexpr float kMaxPitch = 89.9f;
}

Camera::Camera()
{
 updateFromOrbit();
}

Camera::~Camera() = default;

// --- View setup ---

void Camera::lookAt(const float3<float>& eye,
                    const float3<float>& tgt,
                    const float3<float>& up)
{
 position_ = eye;
 target_ = tgt;
 up_ = up;

 // Derive orbit parameters from eye/target
 auto dir = float3<float>{
  target_.x - position_.x,
  target_.y - position_.y,
  target_.z - position_.z
 };
 distance_ = dir.length();
 if (distance_ < kMinDistance) distance_ = kMinDistance;

 // yaw = atan2(x, z), pitch = asin(y / dist)
 yaw_ = std::atan2(dir.x, dir.z) * kRad2Deg;
 float horizDist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
 pitch_ = std::atan2(dir.y, horizDist) * kRad2Deg;
 pitch_ = std::clamp(pitch_, -kMaxPitch, kMaxPitch);
}

// --- Orbit ---

void Camera::orbit(float deltaYaw, float deltaPitch)
{
 yaw_ += deltaYaw;
 pitch_ += deltaPitch;
 pitch_ = std::clamp(pitch_, -kMaxPitch, kMaxPitch);
 updateFromOrbit();
}

float Camera::yaw() const { return yaw_; }
float Camera::yawRadians() const { return yaw_ * kDeg2Rad; }

void Camera::setYaw(float degrees)
{
 yaw_ = degrees;
 updateFromOrbit();
}

float Camera::pitch() const { return pitch_; }
float Camera::pitchRadians() const { return pitch_ * kDeg2Rad; }

void Camera::setPitch(float degrees)
{
 pitch_ = std::clamp(degrees, -kMaxPitch, kMaxPitch);
 updateFromOrbit();
}

float Camera::distance() const { return distance_; }

void Camera::setDistance(float dist)
{
 distance_ = std::max(dist, kMinDistance);
 updateFromOrbit();
}

// --- Pan ---

void Camera::pan(float dx, float dy)
{
 auto r = right();
 auto u = up_;
 target_.x += r.x * dx + u.x * dy;
 target_.y += r.y * dx + u.y * dy;
 target_.z += r.z * dx + u.z * dy;
 updateFromOrbit();
}

// --- Dolly ---

void Camera::dolly(float delta)
{
 distance_ = std::max(distance_ - delta, kMinDistance);
 updateFromOrbit();
}

// --- Projection ---

void Camera::setPerspective(float fovYDegrees, float aspect, float near, float far)
{
 fovY_ = fovYDegrees;
 aspect_ = aspect;
 nearZ_ = near;
 farZ_ = far;
}

void Camera::setFovY(float degrees)
{
 fovY_ = std::clamp(degrees, 1.0f, 179.0f);
}

void Camera::setAspect(float ratio)
{
 aspect_ = std::max(ratio, 0.001f);
}

// --- Accessors ---

float3<float> Camera::forward() const
{
 auto dir = float3<float>{
  target_.x - position_.x,
  target_.y - position_.y,
  target_.z - position_.z
 };
 return dir.normalized();
}

float3<float> Camera::right() const
{
 auto f = forward();
 // right = cross(forward, up)
 return float3<float>{
  f.y * up_.z - f.z * up_.y,
  f.z * up_.x - f.x * up_.z,
  f.x * up_.y - f.y * up_.x
 }.normalized();
}

// --- Matrices ---

glm::mat4 Camera::viewMatrix() const
{
 return glm::lookAt(
  glm::vec3(position_.x, position_.y, position_.z),
  glm::vec3(target_.x, target_.y, target_.z),
  glm::vec3(up_.x, up_.y, up_.z)
 );
}

glm::mat4 Camera::projectionMatrix() const
{
 return glm::perspective(glm::radians(fovY_), aspect_, nearZ_, farZ_);
}

glm::mat4 Camera::viewProjectionMatrix() const
{
 return projectionMatrix() * viewMatrix();
}

// --- Presets ---

void Camera::reset()
{
 target_ = { 0, 0, 0 };
 yaw_ = 0.0f;
 pitch_ = 0.0f;
 distance_ = 5.0f;
 fovY_ = 45.0f;
 aspect_ = 16.0f / 9.0f;
 nearZ_ = 0.1f;
 farZ_ = 1000.0f;
 up_ = { 0, 1, 0 };
 updateFromOrbit();
}

void Camera::frameAll(float boundingRadius)
{
 fitToSphere({ 0, 0, 0 }, boundingRadius);
}

void Camera::setViewFront()
{
 yaw_ = 0.0f;
 pitch_ = 0.0f;
 updateFromOrbit();
}

void Camera::setViewBack()
{
 yaw_ = 180.0f;
 pitch_ = 0.0f;
 updateFromOrbit();
}

void Camera::setViewLeft()
{
 yaw_ = -90.0f;
 pitch_ = 0.0f;
 updateFromOrbit();
}

void Camera::setViewRight()
{
 yaw_ = 90.0f;
 pitch_ = 0.0f;
 updateFromOrbit();
}

void Camera::setViewTop()
{
 yaw_ = 0.0f;
 pitch_ = kMaxPitch;
 updateFromOrbit();
}

void Camera::setViewBottom()
{
 yaw_ = 0.0f;
 pitch_ = -kMaxPitch;
 updateFromOrbit();
}

// --- Fit ---

void Camera::fitToSphere(const float3<float>& center, float radius, float margin)
{
 target_ = center;
 float halfFov = fovY_ * 0.5f * kDeg2Rad;
 float sinHalf = std::sin(halfFov);
 if (sinHalf < 0.001f) sinHalf = 0.001f;
 distance_ = (radius * margin) / sinHalf;
 distance_ = std::max(distance_, kMinDistance);
 updateFromOrbit();
}

// --- Internal ---

void Camera::updateFromOrbit()
{
 float yawRad = yaw_ * kDeg2Rad;
 float pitchRad = pitch_ * kDeg2Rad;

 float cosP = std::cos(pitchRad);
 float sinP = std::sin(pitchRad);
 float cosY = std::cos(yawRad);
 float sinY = std::sin(yawRad);

 position_ = {
  target_.x + distance_ * cosP * sinY,
  target_.y + distance_ * sinP,
  target_.z + distance_ * cosP * cosY
 };
}

}
