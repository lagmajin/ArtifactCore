module;
#include <utility>
#include <cmath>
#include <algorithm>

module Core.Light;

import Float3;

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

void Light::setPosition(const float3<float>& pos)
{
 position_ = pos;
}

// --- Direction ---

void Light::setDirection(const float3<float>& dir)
{
 float len = dir.length();
 if (len > 0.0f) {
  direction_ = { dir.x / len, dir.y / len, dir.z / len };
 }
}

// --- Attenuation ---

void Light::setAttenuation(float constant, float linear, float quadratic)
{
 attenConstant_ = std::max(constant, 0.0f);
 attenLinear_ = std::max(linear, 0.0f);
 attenQuadratic_ = std::max(quadratic, 0.0f);
}

void Light::setRange(float range)
{
 if (range <= 0.0f) {
  attenConstant_ = 1.0f;
  attenLinear_ = 0.0f;
  attenQuadratic_ = 0.0f;
  return;
 }
 // Lighthouse/learnopengl convention:
 // constant=1, linear=4.5/range, quadratic=75/(range^2)
 attenConstant_ = 1.0f;
 attenLinear_ = 4.5f / range;
 attenQuadratic_ = 75.0f / (range * range);
}

// --- Spot cone ---

void Light::setCutoff(float innerDegrees, float outerDegrees)
{
 spotInnerDeg_ = std::clamp(innerDegrees, 0.0f, 90.0f);
 spotOuterDeg_ = std::clamp(outerDegrees, spotInnerDeg_, 90.0f);
}

void Light::setSpotAngle(float degrees)
{
 float d = std::clamp(degrees, 1.0f, 90.0f);
 setCutoff(d * 0.8f, d);
}

// --- Enabled ---

void Light::setEnabled(bool on)
{
 enabled_ = on;
}

// --- Presets ---

Light Light::makeDirectional(const float3<float>& dir,
                              const float3<float>& color,
                              float intensity)
{
 Light l(LightType::Directional);
 l.setDirection(dir);
 l.setColor(color);
 l.setIntensity(intensity);
 return l;
}

Light Light::makePoint(const float3<float>& pos,
                        const float3<float>& color,
                        float intensity,
                        float range)
{
 Light l(LightType::Point);
 l.setPosition(pos);
 l.setColor(color);
 l.setIntensity(intensity);
 l.setRange(range);
 return l;
}

Light Light::makeSpot(const float3<float>& pos,
                       const float3<float>& dir,
                       const float3<float>& color,
                       float intensity,
                       float angle,
                       float range)
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
