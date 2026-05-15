module;
#include <utility>
#include <cmath>
#include <algorithm>

export module Core.Light;

import Float3;

export namespace ArtifactCore
{

 enum class LightType {
  Directional,  // 無限遠の平行光源 (太陽光)
  Point,        // 全方向に放射する点光源
  Spot,         // 円錐形のスポットライト
  Ambient       // 環境光 (方向なし、均等照明)
 };

 /// 3D DCC-style light source.
 class Light {
 public:
  Light();
  explicit Light(LightType type);
  ~Light();

  // --- Type ---

  LightType type() const { return type_; }
  void setType(LightType type);

  // --- Color & Intensity ---

  float3<float> color() const { return color_; }
  void setColor(const float3<float>& rgb);
  float intensity() const { return intensity_; }
  void setIntensity(float value);

  // Effective radiance = color * intensity
  float3<float> radiance() const;

  // --- Position (Point / Spot) ---

  float3<float> position() const { return position_; }
  void setPosition(const float3<float>& pos);

  // --- Direction (Directional / Spot) ---

  float3<float> direction() const { return direction_; }
  void setDirection(const float3<float>& dir);

  // --- Attenuation (Point / Spot) ---

  // Attenuation = 1 / (constant + linear*d + quadratic*d^2)
  float attenuationConstant() const { return attenConstant_; }
  float attenuationLinear() const { return attenLinear_; }
  float attenuationQuadratic() const { return attenQuadratic_; }
  void setAttenuation(float constant, float linear, float quadratic);

  // Preset attenuation by effective range
  void setRange(float range);

  // --- Spot cone (Spot only) ---

  // Inner/outer cutoff in degrees (full bright to falloff edge)
  float spotInnerCutoff() const { return spotInnerDeg_; }
  float spotOuterCutoff() const { return spotOuterDeg_; }
  void setCutoff(float innerDegrees, float outerDegrees);

  // Convenience: set uniform cutoff
  void setSpotAngle(float degrees);

  // --- Enabled ---

  bool enabled() const { return enabled_; }
  void setEnabled(bool on);

  // --- Presets ---

  static Light makeDirectional(const float3<float>& dir,
                                const float3<float>& color = { 1, 1, 1 },
                                float intensity = 1.0f);

  static Light makePoint(const float3<float>& pos,
                          const float3<float>& color = { 1, 1, 1 },
                          float intensity = 1.0f,
                          float range = 10.0f);

  static Light makeSpot(const float3<float>& pos,
                         const float3<float>& dir,
                         const float3<float>& color = { 1, 1, 1 },
                         float intensity = 1.0f,
                         float angle = 45.0f,
                         float range = 10.0f);

  static Light makeAmbient(const float3<float>& color = { 1, 1, 1 },
                            float intensity = 0.1f);

 private:
  LightType type_ = LightType::Directional;
  float3<float> color_ = { 1, 1, 1 };
  float intensity_ = 1.0f;
  float3<float> position_ = { 0, 5, 0 };
  float3<float> direction_ = { 0, -1, 0 };
  float attenConstant_ = 1.0f;
  float attenLinear_ = 0.0f;
  float attenQuadratic_ = 0.0f;
  float spotInnerDeg_ = 30.0f;
  float spotOuterDeg_ = 45.0f;
  bool enabled_ = true;
 };

};
