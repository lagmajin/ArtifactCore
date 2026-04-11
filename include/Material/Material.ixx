module;
#include <utility>
#include <memory>
#include <QColor>
#include <QString>
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

 /// PBR マテリアル。DCC 標準 (glTF / Standard Surface 互換)。
 class Material {
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

  // --- Emission ---
  void setEmissionColor(const QColor& color);
  QColor emissionColor() const;
  void setEmissionStrength(float value);
  float emissionStrength() const;

  // --- Opacity ---
  void setOpacity(float value);
  float opacity() const;

  // --- Normal ---
  void setNormalStrength(float value);
  float normalStrength() const;

  // --- Ambient Occlusion ---
  void setOcclusionStrength(float value);
  float occlusionStrength() const;

  // --- Texture Paths ---
  void setBaseColorTexture(const UniString& path);
  UniString baseColorTexture() const;
  void setMetallicRoughnessTexture(const UniString& path);
  UniString metallicRoughnessTexture() const;
  void setNormalTexture(const UniString& path);
  UniString normalTexture() const;
  void setEmissionTexture(const UniString& path);
  UniString emissionTexture() const;
  void setOcclusionTexture(const UniString& path);
  UniString occlusionTexture() const;
  void setOpacityTexture(const UniString& path);
  UniString opacityTexture() const;

  // --- MaterialX ---
  void setMaterialXDocument(const UniString& xml);
  UniString materialXDocument() const;
  bool hasMaterialXDocument() const;
  void clearMaterialXDocument();

  // --- Presets ---
  static Material makeDefault();
  static Material makeMetal(const QColor& color = QColor(200, 200, 200));
  static Material makePlastic(const QColor& color = QColor(255, 255, 255));
  static Material makeGlass(const QColor& color = QColor(240, 248, 255));
  static Material makeEmissive(const QColor& color = QColor(255, 255, 255),
                                float strength = 5.0f);
 };

}
