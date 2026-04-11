module;
#include <QColor>
#include <QString>
#include <memory>
#include <utility>

export module Material.Material3D;

import Utils.String.UniString;

export namespace ArtifactCore {

/// Basic 3D Material for shading 3D objects.
/// Supports diffuse/specular/roughness properties with optional texture
/// mapping.
class Material3D {
private:
  class Impl;
  Impl *impl_;

public:
  Material3D();
  Material3D(const Material3D &other);
  Material3D(Material3D &&other) noexcept;
  ~Material3D();
  Material3D &operator=(const Material3D &other);
  Material3D &operator=(Material3D &&other) noexcept;

  // --- Identity ---
  void setName(const UniString &name);
  UniString name() const;

  // --- Diffuse ---
  void setDiffuseColor(const QColor &color);
  QColor diffuseColor() const;
  void setDiffuseTexture(const UniString &path);
  UniString diffuseTexture() const;

  // --- Specular ---
  void setSpecularColor(const QColor &color);
  QColor specularColor() const;
  void setSpecularStrength(float value);
  float specularStrength() const;
  void setSpecularTexture(const UniString &path);
  UniString specularTexture() const;

  // --- Roughness ---
  void setRoughness(float value);
  float roughness() const;
  void setRoughnessTexture(const UniString &path);
  UniString roughnessTexture() const;

  // --- Normal ---
  void setNormalStrength(float value);
  float normalStrength() const;
  void setNormalTexture(const UniString &path);
  UniString normalTexture() const;

  // --- MaterialX ---
  void setMaterialXDocument(const UniString &xml);
  UniString materialXDocument() const;
  bool hasMaterialXDocument() const;
  void clearMaterialXDocument();

  // --- Presets ---
  static Material3D makeDefault();
  static Material3D makePlastic(const QColor &color = QColor(255, 255, 255));
  static Material3D makeMetal(const QColor &color = QColor(200, 200, 200));
};

} // namespace ArtifactCore