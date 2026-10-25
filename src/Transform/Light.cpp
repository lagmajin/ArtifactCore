module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <string>

module Core.Light;

import Float3;
import Math.Vec;

namespace ArtifactCore
{

Light::Light() = default;

Light::Light(LightType type) : type_(type) {}

Light::~Light() = default;

// --- Type ---

void Light::setType(LightType type)
{
 type_ = type;
}

// --- Color & Intensity ---

void Light::setColor(const float3<float>& rgb)
{
 color_ = rgb;
}

void Light::setIntensity(float value)
{
 intensity_ = std::max(value, 0.0f);
}

float3<float> Light::radiance() const
{
 return { color_.x * intensity_, color_.y * intensity_, color_.z * intensity_ };
}

// --- Position ---

void Light::setPosition(Coordinates::WorldPoint3 pos)
{
 position_ = pos;
}

// --- Direction ---

void Light::setDirection(Coordinates::WorldVector3 dir)
{
 float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
 if (len > 0.0f) {
  direction_ = dir / len;
 }
}

// --- Attenuation ---

void Light::setAttenuation(float constant, float linear, float quadratic)
{
 attenConstant_ = std::max(constant, 0.0f);
 attenLinear_ = std::max(linear, 0.0f);
 attenQuadratic_ = std::max(quadratic, 0.0f);
}

void Light::setRange(Units::Pixels range)
{
 if (range.value <= 0.0f) {
  attenConstant_ = 1.0f;
  attenLinear_ = 0.0f;
  attenQuadratic_ = 0.0f;
  return;
 }
 // Lighthouse/learnopengl convention:
 // constant=1, linear=4.5/range, quadratic=75/(range^2)
 attenConstant_ = 1.0f;
 attenLinear_ = 4.5f / range.value;
 attenQuadratic_ = 75.0f / (range.value * range.value);
}

void Light::setAreaSize(Units::Pixels width, Units::Pixels height)
{
 areaWidth_ = Units::Pixels{std::max(width.value, 1.0f)};
 areaHeight_ = Units::Pixels{std::max(height.value, 1.0f)};
}

void Light::setAreaShape(int shape)
{
 areaShape_ = std::clamp(shape, 0, 1);
}

// --- Spot cone ---

void Light::setCutoff(Units::Degrees innerAngle, Units::Degrees outerAngle)
{
 spotInnerDeg_ = std::clamp(innerAngle.value, 0.0f, 90.0f);
 spotOuterDeg_ = std::clamp(outerAngle.value, spotInnerDeg_, 90.0f);
}

void Light::setSpotAngle(Units::Degrees degrees)
{
 float d = std::clamp(degrees.value, 1.0f, 90.0f);
 setCutoff(Units::Degrees{d * 0.8f}, Units::Degrees{d});
}

void Light::setGoboTexturePath(std::string path)
{
 goboTexturePath_ = std::move(path);
}

void Light::setGoboIntensity(float value)
{
 goboIntensity_ = std::clamp(value, 0.0f, 1.0f);
}

void Light::setGoboRotation(Units::Degrees degrees)
{
 goboRotation_ = std::fmod(degrees.value, 360.0f);
}

void Light::setGoboInvert(bool enabled)
{
 goboInvert_ = enabled;
}

// --- Shadows ---

void Light::setCastsShadows(bool enabled)
{
 castsShadows_ = enabled;
}

void Light::setShadowSoftness(float value)
{
 if (!std::isfinite(value)) {
  shadowSoftness_ = 0.0f;
  return;
 }
 shadowSoftness_ = std::clamp(value, 0.0f, 2.0f);
}

// --- Enabled ---

void Light::setEnabled(bool on)
{
 enabled_ = on;
}

// --- Presets ---

Light Light::makeDirectional(Coordinates::WorldVector3 dir,
                              const float3<float>& color,
                              float intensity)
{
 Light l(LightType::Directional);
 l.setDirection(dir);
 l.setColor(color);
 l.setIntensity(intensity);
 return l;
}

Light Light::makePoint(Coordinates::WorldPoint3 pos,
                        const float3<float>& color,
                        float intensity,
                        Units::Pixels range)
{
 Light l(LightType::Point);
 l.setPosition(pos);
 l.setColor(color);
 l.setIntensity(intensity);
 l.setRange(range);
 return l;
}

Light Light::makeSpot(Coordinates::WorldPoint3 pos,
                       Coordinates::WorldVector3 dir,
                       const float3<float>& color,
                       float intensity,
                       Units::Degrees angle,
                       Units::Pixels range)
{
 Light l(LightType::Spot);
 l.setPosition(pos);
 l.setDirection(dir);
 l.setColor(color);
 l.setIntensity(intensity);
 l.setSpotAngle(angle);
 l.setRange(range);
 return l;
}

Light Light::makeAmbient(const float3<float>& color, float intensity)
{
 Light l(LightType::Ambient);
 l.setColor(color);
 l.setIntensity(intensity);
 return l;
}

}
