module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <string>

export module Core.Light;

import Float3;
import Math.Vec;

export namespace ArtifactCore
{

 enum class LightType {
  Directional,  // 無限遠の平行光源 (太陽光)
  Point,        // 全方向に放射する点光源
  Spot,         // 円錐形のスポットライト
  Ambient,      // 環境光 (方向なし、均等照明)
  Area          // 面光源（矩形近似）
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

  Coordinates::WorldPoint3 position() const { return position_; }
  void setPosition(Coordinates::WorldPoint3 pos);

  // --- Direction (Directional / Spot) ---

  Coordinates::WorldVector3 direction() const { return direction_; }
  void setDirection(Coordinates::WorldVector3 dir);

  // --- Attenuation (Point / Spot) ---

  // Attenuation = 1 / (constant + linear*d + quadratic*d^2)
  float attenuationConstant() const { return attenConstant_; }
  float attenuationLinear() const { return attenLinear_; }
  float attenuationQuadratic() const { return attenQuadratic_; }
  void setAttenuation(float constant, float linear, float quadratic);

  // Preset attenuation by effective range
  void setRange(Units::Pixels range);
  Units::Pixels areaWidth() const { return areaWidth_; }
  Units::Pixels areaHeight() const { return areaHeight_; }
  int areaShape() const { return areaShape_; }
  void setAreaSize(Units::Pixels width, Units::Pixels height);
  void setAreaShape(int shape);

  // --- Spot cone (Spot only) ---

  // Inner/outer cutoff in degrees (full bright to falloff edge)
  Units::Degrees spotInnerCutoff() const { return {spotInnerDeg_}; }
  Units::Degrees spotOuterCutoff() const { return {spotOuterDeg_}; }
  void setCutoff(Units::Degrees innerAngle, Units::Degrees outerAngle);

  // Convenience: set uniform cutoff
  void setSpotAngle(Units::Degrees degrees);

  // Optional Spot-light projection (GOBO/cookie). The path is deliberately
  // kept in the renderer-neutral light contract; GPU texture ownership stays
  // in MeshRenderer.
  const std::string& goboTexturePath() const { return goboTexturePath_; }
  void setGoboTexturePath(std::string path);
  float goboIntensity() const { return goboIntensity_; }
  void setGoboIntensity(float value);
  Units::Degrees goboRotation() const { return {goboRotation_}; }
  void setGoboRotation(Units::Degrees degrees);
  bool goboInvert() const { return goboInvert_; }
  void setGoboInvert(bool enabled);

  // --- Shadows (renderer-neutral contract) ---
  // castsShadows selects this light as a shadow-map caster in the app layer.
  // shadowSoftness 0 = hard single tap, >0 = 3x3 PCF blend in the PBR shader
  // (MeshRenderer clamps to 0..2). Kept as plain values: no allocation.
  bool castsShadows() const { return castsShadows_; }
  void setCastsShadows(bool enabled);
  float shadowSoftness() const { return shadowSoftness_; }
  void setShadowSoftness(float value);

  // --- Enabled ---

  bool enabled() const { return enabled_; }
  void setEnabled(bool on);

  // --- Presets ---

  static Light makeDirectional(Coordinates::WorldVector3 dir,
                                const float3<float>& color = { 1, 1, 1 },
                                float intensity = 1.0f);

  static Light makePoint(Coordinates::WorldPoint3 pos,
                          const float3<float>& color = { 1, 1, 1 },
                          float intensity = 1.0f,
                          Units::Pixels range = Units::Pixels{10.0f});

  static Light makeSpot(Coordinates::WorldPoint3 pos,
                         Coordinates::WorldVector3 dir,
                         const float3<float>& color = { 1, 1, 1 },
                         float intensity = 1.0f,
                         Units::Degrees angle = Units::Degrees{45.0f},
                         Units::Pixels range = Units::Pixels{10.0f});

  static Light makeAmbient(const float3<float>& color = { 1, 1, 1 },
                            float intensity = 0.1f);

 private:
  LightType type_ = LightType::Directional;
  float3<float> color_ = { 1, 1, 1 };
  float intensity_ = 1.0f;
  Coordinates::WorldPoint3 position_ = { 0, 5, 0 };
  Coordinates::WorldVector3 direction_ = { 0, -1, 0 };
  float attenConstant_ = 1.0f;
  float attenLinear_ = 0.0f;
  float attenQuadratic_ = 0.0f;
  float spotInnerDeg_ = 30.0f;
  float spotOuterDeg_ = 45.0f;
  std::string goboTexturePath_;
  float goboIntensity_ = 1.0f;
  float goboRotation_ = 0.0f;
  bool goboInvert_ = false;
  bool castsShadows_ = true;
  float shadowSoftness_ = 0.0f;
  bool enabled_ = true;
  Units::Pixels areaWidth_{100.0f};
  Units::Pixels areaHeight_{100.0f};
  int areaShape_ = 0;
 };

};
