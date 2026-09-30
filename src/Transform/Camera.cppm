module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

module Core.Camera;

import Float3;
import Math.Vec;

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

void Camera::lookAt(Coordinates::WorldPoint3 eye,
                    Coordinates::WorldPoint3 tgt,
                    Coordinates::WorldVector3 up)
{
 position_ = eye;
 target_ = tgt;
 up_ = up;

 // Derive orbit parameters from eye/target
 const Coordinates::WorldVector3 worldDirection = target_ - position_;
 auto dir = float3<float>{
  worldDirection.x,
  worldDirection.y,
  worldDirection.z
 };
 distance_ = Units::WorldLength{dir.length()};
 if (distance_.value < kMinDistance) distance_ = Units::WorldLength{kMinDistance};

 // yaw = atan2(x, z), pitch = asin(y / dist)
 yaw_ = Units::Degrees{std::atan2(dir.x, dir.z) * kRad2Deg};
 float horizDist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
 pitch_ = Units::Degrees{std::atan2(dir.y, horizDist) * kRad2Deg};
 pitch_ = Units::Degrees{std::clamp(pitch_.value, -kMaxPitch, kMaxPitch)};
}

// --- Orbit ---

void Camera::orbit(Units::Degrees deltaYaw, Units::Degrees deltaPitch)
{
 yaw_ = yaw_ + deltaYaw;
 pitch_ = pitch_ + deltaPitch;
 pitch_ = Units::Degrees{std::clamp(pitch_.value, -kMaxPitch, kMaxPitch)};
 updateFromOrbit();
}

Units::Degrees Camera::yaw() const { return yaw_; }
Units::Radians Camera::yawRadians() const { return Units::toRadians(yaw_); }

void Camera::setYaw(Units::Degrees degrees)
{
 yaw_ = degrees;
 updateFromOrbit();
}

Units::Degrees Camera::pitch() const { return pitch_; }
Units::Radians Camera::pitchRadians() const { return Units::toRadians(pitch_); }

void Camera::setPitch(Units::Degrees degrees)
{
 pitch_ = Units::Degrees{std::clamp(degrees.value, -kMaxPitch, kMaxPitch)};
 updateFromOrbit();
}

Units::WorldLength Camera::distance() const { return distance_; }

void Camera::setDistance(Units::WorldLength dist)
{
 distance_ = Units::WorldLength{std::max(dist.value, kMinDistance)};
 updateFromOrbit();
}

// --- Pan ---

void Camera::pan(Units::WorldLength dx, Units::WorldLength dy)
{
 const auto r = right();
 auto u = up_;
 target_ = target_ + r * dx.value + u * dy.value;
 updateFromOrbit();
}

// --- Dolly ---

void Camera::dolly(Units::WorldLength delta)
{
 distance_ = Units::WorldLength{
     std::max(distance_.value - delta.value, kMinDistance)};
 updateFromOrbit();
}

// --- Projection ---

void Camera::setPerspective(Units::Degrees fovY,
                            float aspect,
                            Units::Pixels nearZ,
                            Units::Pixels farZ)
{
 fovY_ = fovY;
 aspect_ = aspect;
 nearZ_ = nearZ;
 farZ_ = farZ;
}

void Camera::setFovY(Units::Degrees degrees)
{
 fovY_ = Units::Degrees{std::clamp(degrees.value, 1.0f, 179.0f)};
}

void Camera::setAspect(float ratio)
{
 aspect_ = std::max(ratio, 0.001f);
}

// --- Accessors ---

Coordinates::WorldVector3 Camera::forward() const
{
 const auto dir = target_ - position_;
 const auto normalized = float3<float>{dir.x, dir.y, dir.z}.normalized();
 return {normalized.x, normalized.y, normalized.z};
}

Coordinates::WorldVector3 Camera::right() const
{
 auto f = forward();
 // right = cross(forward, up)
 const auto right = float3<float>{
  f.y * up_.z - f.z * up_.y,
  f.z * up_.x - f.x * up_.z,
  f.x * up_.y - f.y * up_.x
 }.normalized();
 return {right.x, right.y, right.z};
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
 return glm::perspective(glm::radians(fovY_.value), aspect_, nearZ_.value,
                         farZ_.value);
}

glm::mat4 Camera::viewProjectionMatrix() const
{
 return projectionMatrix() * viewMatrix();
}

// --- Presets ---

void Camera::reset()
{
 target_ = { 0, 0, 0 };
 yaw_ = Units::Degrees{0.0f};
 pitch_ = Units::Degrees{0.0f};
 distance_ = Units::WorldLength{5.0f};
 fovY_ = Units::Degrees{45.0f};
 aspect_ = 16.0f / 9.0f;
 nearZ_ = Units::Pixels{0.1f};
 farZ_ = Units::Pixels{1000.0f};
 up_ = { 0, 1, 0 };
 updateFromOrbit();
}

void Camera::frameAll(Units::WorldLength boundingRadius)
{
 fitToSphere(Coordinates::WorldPoint3{0, 0, 0}, boundingRadius);
}

void Camera::setViewFront()
{
 yaw_ = Units::Degrees{0.0f};
 pitch_ = Units::Degrees{0.0f};
 updateFromOrbit();
}

void Camera::setViewBack()
{
 yaw_ = Units::Degrees{180.0f};
 pitch_ = Units::Degrees{0.0f};
 updateFromOrbit();
}

void Camera::setViewLeft()
{
 yaw_ = Units::Degrees{-90.0f};
 pitch_ = Units::Degrees{0.0f};
 updateFromOrbit();
}

void Camera::setViewRight()
{
 yaw_ = Units::Degrees{90.0f};
 pitch_ = Units::Degrees{0.0f};
 updateFromOrbit();
}

void Camera::setViewTop()
{
 yaw_ = Units::Degrees{0.0f};
 pitch_ = Units::Degrees{kMaxPitch};
 updateFromOrbit();
}

void Camera::setViewBottom()
{
 yaw_ = Units::Degrees{0.0f};
 pitch_ = Units::Degrees{-kMaxPitch};
 updateFromOrbit();
}

// --- Fit ---

void Camera::fitToSphere(Coordinates::WorldPoint3 center,
                         Units::WorldLength radius,
                         float margin)
{
 target_ = center;
 float halfFov = fovY_.value * 0.5f * kDeg2Rad;
 float sinHalf = std::sin(halfFov);
 if (sinHalf < 0.001f) sinHalf = 0.001f;
 distance_ = Units::WorldLength{(radius.value * margin) / sinHalf};
 distance_ = Units::WorldLength{
     std::max(distance_.value, kMinDistance)};
 updateFromOrbit();
}

// --- Internal ---

void Camera::updateFromOrbit()
{
 float yawRad = yaw_.value * kDeg2Rad;
 float pitchRad = pitch_.value * kDeg2Rad;

 float cosP = std::cos(pitchRad);
 float sinP = std::sin(pitchRad);
 float cosY = std::cos(yawRad);
 float sinY = std::sin(yawRad);

 position_ = target_ + Coordinates::WorldVector3{
  distance_.value * cosP * sinY,
  distance_.value * sinP,
  distance_.value * cosP * cosY
 };
}

}
