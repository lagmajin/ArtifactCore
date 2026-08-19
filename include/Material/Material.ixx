module;
#include <utility>
#include <memory>
#include <QColor>
#include <QString>
#include <QFile>
#include <QVector3D>
export module Material.Material;

import Utils.String.UniString;

export namespace ArtifactCore {

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
  void setSpecular(float value);
  float specular() const;
  void setIOR(float value);
  float ior() const;
  void setTransmission(float value);
  float transmission() const;
  void setClearcoat(float value);
  float clearcoat() const;
  void setClearcoatRoughness(float value);
  float clearcoatRoughness() const;

  // --- Emission ---
  void setEmissionColor(const QColor& color);
  QColor emissionColor() const;
  void setEmissionStrength(float value);
  float emissionStrength() const;

  // --- Opacity ---
  void setOpacity(float value);
  float opacity() const;
  void setAlphaMode(MaterialAlphaMode mode);
  MaterialAlphaMode alphaMode() const;

  // --- Normal ---
  void setNormalStrength(float value);
  float normalStrength() const;

  // --- Ambient Occlusion ---
  void setOcclusionStrength(float value);
  float occlusionStrength() const;

  // --- Texture Paths ---
  void setBaseColorTexture(const UniString& path);
  UniString baseColorTexture() const;
  bool hasBaseColorTexture() const;
  void setMetallicRoughnessTexture(const UniString& path);
  UniString metallicRoughnessTexture() const;
  bool hasMetallicRoughnessTexture() const;
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

  // --- MaterialX ---
  void setMaterialXDocument(const UniString& xml);
  UniString materialXDocument() const;
  bool saveMaterialXDocument(const QString& filePath) const;
  bool loadMaterialXDocument(const QString& filePath);

  // --- Presets ---
  static Material makeDefault();
  static Material makeMetal(const QColor& color = QColor(200, 200, 200));
  static Material makePlastic(const QColor& color = QColor(255, 255, 255));
  static Material makeGlass(const QColor& color = QColor(240, 248, 255));
  static Material makeEmissive(const QColor& color = QColor(255, 255, 255),
                                float strength = 5.0f);
 };

 using SceneMaterial = Material;

}
