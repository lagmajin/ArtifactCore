module;
#include <utility>
#include <memory>
#include <QColor>
#include <QString>
#include <QFile>
#include <QVector3D>
export module Material.Material;

import Utils.String.UniString;

namespace ArtifactCore {

export enum class MaterialType {
 Standard,
 PBR,
 Unlit,
 MaterialX
};

export enum class MaterialAlphaMode {
 Opaque,
 Masked,
 Blended
};

export enum class MaterialPresetKind { Matte, Metal, Plastic, Glass };

export struct MaterialTextureUvTransform {
  float scaleU = 1.0f;
  float scaleV = 1.0f;
  float rotationDegrees = 0.0f;
  float offsetU = 0.0f;
  float offsetV = 0.0f;
  int uvSet = 0;
};

 /// PBR マテリアル。DCC 標準 (glTF / Standard Surface 互換)。
 export class Material {
 private:
  class Impl;
  Impl* impl_;
 public:
  Material();
  Material(MaterialType type);
  Material(const Material& other);
  Material(Material&& other) noexcept;
  ~Material();
  Material& operator=(const Material& other);
  Material& operator=(Material&& other) noexcept;

  // --- Identity ---
  void setName(const UniString& name);
  UniString name() const;
  void setType(MaterialType type);
  MaterialType type() const;

  // --- PBR Base ---
  void setBaseColor(const QColor& color);
  QColor baseColor() const;
  void setMetallic(float value);
  float metallic() const;
  void setRoughness(float value);
  float roughness() const;
  void setDiffuseRoughness(float value);
  float diffuseRoughness() const;
  void setSpecular(float value);
  float specular() const;
  void setSpecularTint(const QColor& color);
  QColor specularTint() const;
  void setThinFilm(float thicknessNanometers, float ior);
  float thinFilmThickness() const;
  float thinFilmIOR() const;
  void setIOR(float value);
  float ior() const;
  void setTransmission(float value);
  float transmission() const;
  void setThinWall(bool enabled);
  bool thinWall() const;
  void setClearcoat(float value);
  float clearcoat() const;
  void setClearcoatTexture(const UniString& path);
  UniString clearcoatTexture() const;
  bool hasClearcoatTexture() const;
  void setClearcoatTextureUvScale(float value);
  float clearcoatTextureUvScale() const;
  void setClearcoatTextureUvOffsetU(float value);
  float clearcoatTextureUvOffsetU() const;
  void setClearcoatTextureUvOffsetV(float value);
  float clearcoatTextureUvOffsetV() const;
  void setClearcoatTextureUvRotationDegrees(float value);
  float clearcoatTextureUvRotationDegrees() const;
  void setClearcoatTextureUvSet(int value);
  int clearcoatTextureUvSet() const;
  void setClearcoatRoughnessTexture(const UniString& path);
  UniString clearcoatRoughnessTexture() const;
  void setClearcoatRoughnessTextureUvScale(float value);
  float clearcoatRoughnessTextureUvScale() const;
  void setClearcoatRoughnessTextureUvOffsetU(float value);
  float clearcoatRoughnessTextureUvOffsetU() const;
  void setClearcoatRoughnessTextureUvOffsetV(float value);
  float clearcoatRoughnessTextureUvOffsetV() const;
  void setClearcoatRoughnessTextureUvRotationDegrees(float value);
  float clearcoatRoughnessTextureUvRotationDegrees() const;
  void setClearcoatRoughnessTextureUvSet(int value);
  int clearcoatRoughnessTextureUvSet() const;
  void setClearcoatRoughness(float value);
  float clearcoatRoughness() const;
  void setClearcoatIOR(float value);
  float clearcoatIOR() const;
  void setClearcoatTint(const QColor& color);
  QColor clearcoatTint() const;
  void setClearcoatNormalStrength(float value);
  float clearcoatNormalStrength() const;
  void setClearcoatNormalTexture(const UniString& path);
  UniString clearcoatNormalTexture() const;
  void setClearcoatNormalUvRotationDegrees(float value);
  float clearcoatNormalUvRotationDegrees() const;
  void setClearcoatNormalUvScale(float value);
  float clearcoatNormalUvScale() const;
  void setClearcoatNormalUvOffsetU(float value);
  float clearcoatNormalUvOffsetU() const;
  void setClearcoatNormalUvOffsetV(float value);
  float clearcoatNormalUvOffsetV() const;
  void setClearcoatNormalUvSet(int value);
  int clearcoatNormalUvSet() const;
  void setSheen(float value);
  float sheen() const;
  void setSheenRoughness(float value);
  float sheenRoughness() const;
  void setSheenTint(const QColor& color);
  QColor sheenTint() const;
  void setSheenColorTexture(const UniString& path);
  UniString sheenColorTexture() const;
  void setSheenColorTextureUvTransform(
      const MaterialTextureUvTransform& transform);
  MaterialTextureUvTransform sheenColorTextureUvTransform() const;
  void setSubsurface(float value);
  float subsurface() const;
  void setSubsurfaceIOR(float value);
  float subsurfaceIOR() const;
  void setSubsurfaceAnisotropy(float value);
  float subsurfaceAnisotropy() const;
  void setSubsurfaceColor(const QColor& color);
  QColor subsurfaceColor() const;
  void setSubsurfaceRadius(const QVector3D& radius);
  QVector3D subsurfaceRadius() const;
  void setAnisotropy(float value, float rotationDegrees);
  float anisotropy() const;
  float anisotropyRotationDegrees() const;

  // --- Emission ---
  void setEmissionColor(const QColor& color);
  QColor emissionColor() const;
  void setEmissionStrength(float value);
  float emissionStrength() const;
  void setEmissionTextureUvScale(float value);
  float emissionTextureUvScale() const;
  void setEmissionTextureUvOffsetU(float value);
  float emissionTextureUvOffsetU() const;
  void setEmissionTextureUvOffsetV(float value);
  float emissionTextureUvOffsetV() const;
  void setEmissionTextureUvRotationDegrees(float value);
  float emissionTextureUvRotationDegrees() const;
  void setEmissionTextureUvSet(int value);
  int emissionTextureUvSet() const;

  // --- Opacity ---
  void setOpacity(float value);
  float opacity() const;
  void setOpacityTextureUvScale(float value);
  float opacityTextureUvScale() const;
  void setOpacityTextureUvOffsetU(float value);
  float opacityTextureUvOffsetU() const;
  void setOpacityTextureUvOffsetV(float value);
  float opacityTextureUvOffsetV() const;
  void setOpacityTextureUvRotationDegrees(float value);
  float opacityTextureUvRotationDegrees() const;
  void setOpacityTextureUvSet(int value);
  int opacityTextureUvSet() const;
  void setAlphaMode(MaterialAlphaMode mode);
  MaterialAlphaMode alphaMode() const;
  void setAlphaCutoff(float value);
  float alphaCutoff() const;

  // --- Normal ---
  void setNormalStrength(float value);
  float normalStrength() const;
  void setNormalUvRotationDegrees(float value);
  float normalUvRotationDegrees() const;
  void setNormalUvScale(float value);
  float normalUvScale() const;
  void setNormalUvOffsetU(float value);
  float normalUvOffsetU() const;
  void setNormalUvOffsetV(float value);
  float normalUvOffsetV() const;
  // Selects UV0 (0) or UV1 (1) for the Normal map.
  void setNormalUvSet(int value);
  int normalUvSet() const;

  // --- Ambient Occlusion ---
  void setOcclusionStrength(float value);
  float occlusionStrength() const;
  void setOcclusionTextureUvScale(float value);
  float occlusionTextureUvScale() const;
  void setOcclusionTextureUvOffsetU(float value);
  float occlusionTextureUvOffsetU() const;
  void setOcclusionTextureUvOffsetV(float value);
  float occlusionTextureUvOffsetV() const;
  void setOcclusionTextureUvRotationDegrees(float value);
  float occlusionTextureUvRotationDegrees() const;
  void setOcclusionTextureUvSet(int value);
  int occlusionTextureUvSet() const;

  // --- Texture Paths ---
  void setBaseColorTexture(const UniString& path);
  UniString baseColorTexture() const;
  bool hasBaseColorTexture() const;
  void setBaseColorUvScale(float value);
  float baseColorUvScale() const;
  void setBaseColorUvRotationDegrees(float value);
  float baseColorUvRotationDegrees() const;
  void setBaseColorUvOffsetU(float value);
  float baseColorUvOffsetU() const;
  void setBaseColorUvOffsetV(float value);
  float baseColorUvOffsetV() const;
  void setBaseColorUvSet(int value);
  int baseColorUvSet() const;
  void setMetallicRoughnessUvScale(float value);
  float metallicRoughnessUvScale() const;
  void setMetallicRoughnessUvOffsetU(float value);
  float metallicRoughnessUvOffsetU() const;
  void setMetallicRoughnessUvOffsetV(float value);
  float metallicRoughnessUvOffsetV() const;
  void setMetallicRoughnessUvRotationDegrees(float value);
  float metallicRoughnessUvRotationDegrees() const;
  void setMetallicRoughnessUvSet(int value);
  int metallicRoughnessUvSet() const;
  void setMetallicRoughnessTexture(const UniString& path);
  UniString metallicRoughnessTexture() const;
  bool hasMetallicRoughnessTexture() const;
  void setMetallicTexture(const UniString& path);
  UniString metallicTexture() const;
  bool hasMetallicTexture() const;
  void setMetallicTextureUvTransform(
      const MaterialTextureUvTransform& transform);
  MaterialTextureUvTransform metallicTextureUvTransform() const;
  void setRoughnessTexture(const UniString& path);
  UniString roughnessTexture() const;
  bool hasRoughnessTexture() const;
  void setRoughnessTextureUvTransform(
      const MaterialTextureUvTransform& transform);
  MaterialTextureUvTransform roughnessTextureUvTransform() const;
  void setNormalTexture(const UniString& path);
  UniString normalTexture() const;
  bool hasNormalTexture() const;
  void setEmissionTexture(const UniString& path);
  UniString emissionTexture() const;
  bool hasEmissionTexture() const;
  void setOcclusionTexture(const UniString& path);
  UniString occlusionTexture() const;
  bool hasOcclusionTexture() const;
  void setOpacityTexture(const UniString& path);
  UniString opacityTexture() const;
  bool hasOpacityTexture() const;

  // --- Texture Transform (shared UV offset/scale/rotation) ---
  // glTF KHR_texture_transform相当を全テクスチャ共通で適用する。
  // offsetはUV加算、scaleはUV乗算、rotationは度数法(反時計回り)。
  void setUvOffsetU(float value);
  float uvOffsetU() const;
  void setUvOffsetV(float value);
  float uvOffsetV() const;
  void setUvScaleU(float value);
  float uvScaleU() const;
  void setUvScaleV(float value);
  float uvScaleV() const;
  void setUvRotationDegrees(float value);
  float uvRotationDegrees() const;

  // --- Environment ---
  // Per-material IBL intensity scale (multiplies the global environment
  // intensity for indirect diffuse/specular/transmission on this material).
  void setEnvironmentIntensity(float value);
  float environmentIntensity() const;

  // --- MaterialX ---
  void setMaterialXDocument(const UniString& xml);
  UniString materialXDocument() const;
  bool saveMaterialXDocument(const QString& filePath) const;
  bool loadMaterialXDocument(const QString& filePath);

  // --- Material graph (Blender-style node JSON, ShaderNode round-trip) ---
  // Carried through drawMesh so the renderer applies it on change.
  void setMaterialGraphJson(const UniString& json);
  UniString materialGraphJson() const;
  bool hasMaterialGraphJson() const;

  // --- Presets ---
  static Material makeDefault();
  static Material makeMetal(const QColor& color = QColor(200, 200, 200));
  static Material makePlastic(const QColor& color = QColor(255, 255, 255));
  static Material makeGlass(const QColor& color = QColor(240, 248, 255));
  static Material makeEmissive(const QColor& color = QColor(255, 255, 255),
                                float strength = 5.0f);
  // Canonical ArtifactStudio layer presets. Kept separate from the older
  // generic factories above so their established defaults remain compatible.
  static Material makeStudioPreset(MaterialPresetKind preset);
 };

}
