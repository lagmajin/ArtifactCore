module;
#include <QString>
#include <QFile>
#include <QColor>
#include <QVector3D>
#include <algorithm>
#include <cmath>

module Material.Material;

import Utils.String.UniString;

namespace ArtifactCore {

class Material::Impl {
public:
 UniString name_;
 MaterialType type_ = MaterialType::PBR;

 // PBR Base
 QColor baseColor_ = QColor(255, 255, 255);
 float metallic_ = 0.0f;
 float roughness_ = 0.5f;
 float diffuseRoughness_ = 0.0f;
 float specular_ = 0.5f;
 QColor specularTint_ = QColor(255, 255, 255);
 float thinFilmThickness_ = 0.0f;
 float thinFilmIOR_ = 1.33f;
 float ior_ = 1.5f;
 float transmission_ = 0.0f;
 float clearcoat_ = 0.0f;
 float clearcoatRoughness_ = 0.03f;
 float clearcoatIOR_ = 1.5f;
 QColor clearcoatTint_ = QColor(255, 255, 255);
 float clearcoatNormalStrength_ = 1.0f;
 UniString clearcoatNormalTexture_;
 float clearcoatNormalUvRotationDegrees_ = 0.0f;
 float clearcoatNormalUvScale_ = 1.0f;
 float clearcoatNormalUvOffsetU_ = 0.0f;
 float clearcoatNormalUvOffsetV_ = 0.0f;
 float sheen_ = 0.0f;
 float sheenRoughness_ = 0.5f;
 QColor sheenTint_ = QColor(255, 255, 255);
 float subsurface_ = 0.0f;
 float subsurfaceIOR_ = 1.4f;
 float subsurfaceAnisotropy_ = 0.0f;
 QColor subsurfaceColor_ = QColor(255, 255, 255);
 QVector3D subsurfaceRadius_{1.0f, 0.2f, 0.1f};
 float anisotropy_ = 0.0f;
 float anisotropyRotationDegrees_ = 0.0f;

 // Emission
 QColor emissionColor_ = QColor(0, 0, 0);
 float emissionStrength_ = 0.0f;

 // Opacity
 float opacity_ = 1.0f;
 MaterialAlphaMode alphaMode_ = MaterialAlphaMode::Opaque;
 float alphaCutoff_ = 0.5f;

 // Normal
 float normalStrength_ = 1.0f;
  float normalUvRotationDegrees_ = 0.0f;
  float normalUvScale_ = 1.0f;
  float normalUvOffsetU_ = 0.0f;
  float normalUvOffsetV_ = 0.0f;
  int normalUvSet_ = 0;

 // AO
 float occlusionStrength_ = 1.0f;
 float occlusionTextureUvScale_ = 1.0f;
 float occlusionTextureUvOffsetU_ = 0.0f;
 float occlusionTextureUvOffsetV_ = 0.0f;
 float occlusionTextureUvRotationDegrees_ = 0.0f;

 // Texture paths
 UniString baseColorTexture_;
 bool hasBaseColorTexture_ = false;
 float metallicRoughnessUvScale_ = 1.0f;
 float metallicRoughnessUvOffsetU_ = 0.0f;
 float metallicRoughnessUvOffsetV_ = 0.0f;
 float metallicRoughnessUvRotationDegrees_ = 0.0f;
 float baseColorUvScale_ = 1.0f;
 float baseColorUvRotationDegrees_ = 0.0f;
 float baseColorUvOffsetU_ = 0.0f;
 float baseColorUvOffsetV_ = 0.0f;
 int baseColorUvSet_ = 0;
 UniString metallicRoughnessTexture_;
 bool hasMetallicRoughnessTexture_ = false;
 UniString normalTexture_;
 bool hasNormalTexture_ = false;
 UniString emissionTexture_;
 bool hasEmissionTexture_ = false;
 float emissionTextureUvScale_ = 1.0f;
 float emissionTextureUvOffsetU_ = 0.0f;
 float emissionTextureUvOffsetV_ = 0.0f;
 float emissionTextureUvRotationDegrees_ = 0.0f;
 UniString occlusionTexture_;
 bool hasOcclusionTexture_ = false;
 UniString opacityTexture_;
 bool hasOpacityTexture_ = false;
 float opacityTextureUvScale_ = 1.0f;
 float opacityTextureUvOffsetU_ = 0.0f;
 float opacityTextureUvOffsetV_ = 0.0f;
 float opacityTextureUvRotationDegrees_ = 0.0f;

  // Texture transform (shared UV offset/scale/rotation)
  float uvOffsetU_ = 0.0f;
  float uvOffsetV_ = 0.0f;
  float uvScaleU_ = 1.0f;
  float uvScaleV_ = 1.0f;
  float uvRotationDegrees_ = 0.0f;

  // Environment
  float environmentIntensity_ = 1.0f;

// MaterialX
  UniString materialXDocument_;

  // Material graph (ShaderNode JSON round-trip, carried to the renderer)
  UniString materialGraphJson_;
};

Material::Material() : impl_(new Impl()) {}
Material::Material(MaterialType type) : impl_(new Impl()) { impl_->type_ = type; }
Material::Material(const Material& other) : impl_(new Impl(*other.impl_)) {}
Material::Material(Material&& other) noexcept : impl_(other.impl_) { other.impl_ = nullptr; }
Material::~Material() { delete impl_; }
Material& Material::operator=(const Material& other) { if(this!=&other){ *impl_ = *other.impl_; } return *this; }
Material& Material::operator=(Material&& other) noexcept { if(this!=&other){ delete impl_; impl_ = other.impl_; other.impl_ = nullptr; } return *this; }

// Texture transform
void Material::setUvOffsetU(float v) {
  impl_->uvOffsetU_ = std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::uvOffsetU() const { return impl_->uvOffsetU_; }
void Material::setUvOffsetV(float v) {
  impl_->uvOffsetV_ = std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::uvOffsetV() const { return impl_->uvOffsetV_; }
void Material::setUvScaleU(float v) {
  impl_->uvScaleU_ = std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::uvScaleU() const { return impl_->uvScaleU_; }
void Material::setUvScaleV(float v) {
  impl_->uvScaleV_ = std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::uvScaleV() const { return impl_->uvScaleV_; }
void Material::setUvRotationDegrees(float v) {
  impl_->uvRotationDegrees_ = std::isfinite(v) ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::uvRotationDegrees() const { return impl_->uvRotationDegrees_; }
void Material::setBaseColorUvScale(float v) {
  impl_->baseColorUvScale_ = std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::baseColorUvScale() const { return impl_->baseColorUvScale_; }
void Material::setBaseColorUvRotationDegrees(float v) {
  impl_->baseColorUvRotationDegrees_ =
      std::isfinite(v) ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::baseColorUvRotationDegrees() const {
  return impl_->baseColorUvRotationDegrees_;
}
void Material::setBaseColorUvOffsetU(float v) {
  impl_->baseColorUvOffsetU_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::baseColorUvOffsetU() const {
  return impl_->baseColorUvOffsetU_;
}
void Material::setBaseColorUvOffsetV(float v) {
  impl_->baseColorUvOffsetV_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::baseColorUvOffsetV() const {
  return impl_->baseColorUvOffsetV_;
}
void Material::setBaseColorUvSet(int value) {
  impl_->baseColorUvSet_ = value == 1 ? 1 : 0;
}
int Material::baseColorUvSet() const { return impl_->baseColorUvSet_; }
void Material::setMetallicRoughnessUvScale(float v) {
  impl_->metallicRoughnessUvScale_ =
      std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::metallicRoughnessUvScale() const {
  return impl_->metallicRoughnessUvScale_;
}
void Material::setMetallicRoughnessUvOffsetU(float v) {
  impl_->metallicRoughnessUvOffsetU_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::metallicRoughnessUvOffsetU() const {
  return impl_->metallicRoughnessUvOffsetU_;
}
void Material::setMetallicRoughnessUvOffsetV(float v) {
  impl_->metallicRoughnessUvOffsetV_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::metallicRoughnessUvOffsetV() const {
  return impl_->metallicRoughnessUvOffsetV_;
}
void Material::setMetallicRoughnessUvRotationDegrees(float v) {
  impl_->metallicRoughnessUvRotationDegrees_ =
      std::isfinite(v) ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::metallicRoughnessUvRotationDegrees() const {
  return impl_->metallicRoughnessUvRotationDegrees_;
}

// Environment
void Material::setEnvironmentIntensity(float v) {
  impl_->environmentIntensity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 4.0f) : 1.0f;
}
float Material::environmentIntensity() const { return impl_->environmentIntensity_; }

// Identity
void Material::setName(const UniString& name) { impl_->name_ = name; }
UniString Material::name() const { return impl_->name_; }
void Material::setType(MaterialType type) { impl_->type_ = type; }
MaterialType Material::type() const { return impl_->type_; }

// PBR Base
void Material::setBaseColor(const QColor& color) {
 impl_->baseColor_ = color.isValid() ? color : QColor(255, 255, 255);
}

QColor Material::baseColor() const { return impl_->baseColor_; }
void Material::setMetallic(float v) {
 impl_->metallic_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}

float Material::metallic() const { return impl_->metallic_; }
void Material::setRoughness(float v) {
 impl_->roughness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
}
float Material::roughness() const { return impl_->roughness_; }
void Material::setDiffuseRoughness(float v) {
 impl_->diffuseRoughness_ =
     std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}
float Material::diffuseRoughness() const { return impl_->diffuseRoughness_; }
void Material::setSpecular(float v) {
 impl_->specular_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
}
float Material::specular() const { return impl_->specular_; }
void Material::setSpecularTint(const QColor& color) {
 impl_->specularTint_ = color.isValid() ? color : QColor(255, 255, 255);
}
QColor Material::specularTint() const { return impl_->specularTint_; }
void Material::setThinFilm(float thicknessNanometers, float ior) {
 impl_->thinFilmThickness_ = std::isfinite(thicknessNanometers)
     ? std::clamp(thicknessNanometers, 0.0f, 1000.0f) : 0.0f;
 impl_->thinFilmIOR_ = std::isfinite(ior) ? std::clamp(ior, 1.0f, 2.5f) : 1.33f;
}
float Material::thinFilmThickness() const { return impl_->thinFilmThickness_; }
float Material::thinFilmIOR() const { return impl_->thinFilmIOR_; }
void Material::setIOR(float v) {
 impl_->ior_ = std::isfinite(v) ? std::clamp(v, 1.0f, 3.0f) : 1.5f;
}
float Material::ior() const { return impl_->ior_; }
void Material::setTransmission(float v) {
 impl_->transmission_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}
float Material::transmission() const { return impl_->transmission_; }
void Material::setClearcoat(float v) {
 impl_->clearcoat_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}
float Material::clearcoat() const { return impl_->clearcoat_; }
void Material::setClearcoatRoughness(float v) {
 impl_->clearcoatRoughness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.03f;
}
float Material::clearcoatRoughness() const { return impl_->clearcoatRoughness_; }
void Material::setClearcoatIOR(float v) {
 impl_->clearcoatIOR_ = std::isfinite(v) ? std::clamp(v, 1.0f, 2.0f) : 1.5f;
}
float Material::clearcoatIOR() const { return impl_->clearcoatIOR_; }
void Material::setClearcoatTint(const QColor& color) {
 impl_->clearcoatTint_ = color.isValid() ? color : QColor(255, 255, 255);
}
QColor Material::clearcoatTint() const { return impl_->clearcoatTint_; }
void Material::setClearcoatNormalStrength(float v) {
 impl_->clearcoatNormalStrength_ =
     std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
}
float Material::clearcoatNormalStrength() const {
 return impl_->clearcoatNormalStrength_;
}
void Material::setClearcoatNormalTexture(const UniString& path) {
 impl_->clearcoatNormalTexture_ = path;
}
UniString Material::clearcoatNormalTexture() const {
 return impl_->clearcoatNormalTexture_;
}
void Material::setClearcoatNormalUvRotationDegrees(float v) {
 impl_->clearcoatNormalUvRotationDegrees_ = std::isfinite(v)
     ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::clearcoatNormalUvRotationDegrees() const {
 return impl_->clearcoatNormalUvRotationDegrees_;
}
void Material::setClearcoatNormalUvScale(float v) {
 impl_->clearcoatNormalUvScale_ = std::isfinite(v)
     ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::clearcoatNormalUvScale() const {
 return impl_->clearcoatNormalUvScale_;
}
void Material::setClearcoatNormalUvOffsetU(float v) {
 impl_->clearcoatNormalUvOffsetU_ =
     std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::clearcoatNormalUvOffsetU() const {
 return impl_->clearcoatNormalUvOffsetU_;
}
void Material::setClearcoatNormalUvOffsetV(float v) {
 impl_->clearcoatNormalUvOffsetV_ =
     std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::clearcoatNormalUvOffsetV() const {
 return impl_->clearcoatNormalUvOffsetV_;
}
void Material::setSheen(float v) {
 impl_->sheen_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}
float Material::sheen() const { return impl_->sheen_; }
void Material::setSheenRoughness(float v) {
 impl_->sheenRoughness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
}
float Material::sheenRoughness() const { return impl_->sheenRoughness_; }
void Material::setSheenTint(const QColor& color) {
 impl_->sheenTint_ = color.isValid() ? color : QColor(255, 255, 255);
}
QColor Material::sheenTint() const { return impl_->sheenTint_; }
void Material::setSubsurface(float v) {
 impl_->subsurface_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
}
float Material::subsurface() const { return impl_->subsurface_; }
void Material::setSubsurfaceIOR(float v) {
 impl_->subsurfaceIOR_ = std::isfinite(v) ? std::clamp(v, 1.0f, 2.0f) : 1.4f;
}
float Material::subsurfaceIOR() const { return impl_->subsurfaceIOR_; }
void Material::setSubsurfaceAnisotropy(float v) {
 impl_->subsurfaceAnisotropy_ = std::isfinite(v)
     ? std::clamp(v, -0.9f, 0.9f) : 0.0f;
}
float Material::subsurfaceAnisotropy() const {
 return impl_->subsurfaceAnisotropy_;
}
void Material::setSubsurfaceColor(const QColor& color) {
 impl_->subsurfaceColor_ = color.isValid() ? color : QColor(255, 255, 255);
}
QColor Material::subsurfaceColor() const { return impl_->subsurfaceColor_; }
void Material::setSubsurfaceRadius(const QVector3D& radius) {
 impl_->subsurfaceRadius_ = QVector3D(
     std::isfinite(radius.x()) ? std::clamp(radius.x(), 0.0f, 10.0f) : 1.0f,
     std::isfinite(radius.y()) ? std::clamp(radius.y(), 0.0f, 10.0f) : 0.2f,
     std::isfinite(radius.z()) ? std::clamp(radius.z(), 0.0f, 10.0f) : 0.1f);
}
QVector3D Material::subsurfaceRadius() const { return impl_->subsurfaceRadius_; }
void Material::setAnisotropy(float v, float rotationDegrees) {
 impl_->anisotropy_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f;
 impl_->anisotropyRotationDegrees_ = std::isfinite(rotationDegrees)
     ? std::clamp(rotationDegrees, -180.0f, 180.0f) : 0.0f;
}
float Material::anisotropy() const { return impl_->anisotropy_; }
float Material::anisotropyRotationDegrees() const {
 return impl_->anisotropyRotationDegrees_;
}

// Emission
void Material::setEmissionColor(const QColor& color) {
 impl_->emissionColor_ = color.isValid() ? color : QColor(0, 0, 0);
}
QColor Material::emissionColor() const { return impl_->emissionColor_; }
void Material::setEmissionStrength(float v) {
 impl_->emissionStrength_ = std::isfinite(v) ? std::max(v, 0.0f) : 0.0f;
}
float Material::emissionStrength() const { return impl_->emissionStrength_; }
void Material::setEmissionTextureUvScale(float v) {
  impl_->emissionTextureUvScale_ =
      std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::emissionTextureUvScale() const {
  return impl_->emissionTextureUvScale_;
}
void Material::setEmissionTextureUvOffsetU(float v) {
  impl_->emissionTextureUvOffsetU_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::emissionTextureUvOffsetU() const {
  return impl_->emissionTextureUvOffsetU_;
}
void Material::setEmissionTextureUvOffsetV(float v) {
  impl_->emissionTextureUvOffsetV_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::emissionTextureUvOffsetV() const {
  return impl_->emissionTextureUvOffsetV_;
}
void Material::setEmissionTextureUvRotationDegrees(float v) {
  impl_->emissionTextureUvRotationDegrees_ =
      std::isfinite(v) ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::emissionTextureUvRotationDegrees() const {
  return impl_->emissionTextureUvRotationDegrees_;
}

// Opacity
void Material::setOpacity(float v) {
 impl_->opacity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
}
float Material::opacity() const { return impl_->opacity_; }
void Material::setOpacityTextureUvScale(float v) {
  impl_->opacityTextureUvScale_ =
      std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::opacityTextureUvScale() const {
  return impl_->opacityTextureUvScale_;
}
void Material::setOpacityTextureUvOffsetU(float v) {
  impl_->opacityTextureUvOffsetU_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::opacityTextureUvOffsetU() const {
  return impl_->opacityTextureUvOffsetU_;
}
void Material::setOpacityTextureUvOffsetV(float v) {
  impl_->opacityTextureUvOffsetV_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::opacityTextureUvOffsetV() const {
  return impl_->opacityTextureUvOffsetV_;
}
void Material::setOpacityTextureUvRotationDegrees(float v) {
  impl_->opacityTextureUvRotationDegrees_ =
      std::isfinite(v) ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::opacityTextureUvRotationDegrees() const {
  return impl_->opacityTextureUvRotationDegrees_;
}
void Material::setAlphaMode(MaterialAlphaMode mode) { impl_->alphaMode_ = mode; }
MaterialAlphaMode Material::alphaMode() const { return impl_->alphaMode_; }
void Material::setAlphaCutoff(float v) {
 impl_->alphaCutoff_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
}
float Material::alphaCutoff() const { return impl_->alphaCutoff_; }

// Normal
void Material::setNormalStrength(float v) {
 impl_->normalStrength_ = std::isfinite(v) ? std::max(v, 0.0f) : 1.0f;
}
float Material::normalStrength() const { return impl_->normalStrength_; }
void Material::setNormalUvRotationDegrees(float v) {
  impl_->normalUvRotationDegrees_ = std::isfinite(v)
      ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::normalUvRotationDegrees() const {
  return impl_->normalUvRotationDegrees_;
}
void Material::setNormalUvScale(float v) {
  impl_->normalUvScale_ = std::isfinite(v)
      ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::normalUvScale() const { return impl_->normalUvScale_; }
void Material::setNormalUvOffsetU(float v) {
  impl_->normalUvOffsetU_ = std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::normalUvOffsetU() const { return impl_->normalUvOffsetU_; }
void Material::setNormalUvOffsetV(float v) {
  impl_->normalUvOffsetV_ = std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::normalUvOffsetV() const { return impl_->normalUvOffsetV_; }
void Material::setNormalUvSet(int value) {
  impl_->normalUvSet_ = value == 1 ? 1 : 0;
}
int Material::normalUvSet() const { return impl_->normalUvSet_; }

// AO
void Material::setOcclusionStrength(float v) {
 impl_->occlusionStrength_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
}
float Material::occlusionStrength() const { return impl_->occlusionStrength_; }
void Material::setOcclusionTextureUvScale(float v) {
  impl_->occlusionTextureUvScale_ =
      std::isfinite(v) ? std::clamp(v, 0.01f, 10.0f) : 1.0f;
}
float Material::occlusionTextureUvScale() const {
  return impl_->occlusionTextureUvScale_;
}
void Material::setOcclusionTextureUvOffsetU(float v) {
  impl_->occlusionTextureUvOffsetU_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::occlusionTextureUvOffsetU() const {
  return impl_->occlusionTextureUvOffsetU_;
}
void Material::setOcclusionTextureUvOffsetV(float v) {
  impl_->occlusionTextureUvOffsetV_ =
      std::isfinite(v) ? std::clamp(v, -10.0f, 10.0f) : 0.0f;
}
float Material::occlusionTextureUvOffsetV() const {
  return impl_->occlusionTextureUvOffsetV_;
}
void Material::setOcclusionTextureUvRotationDegrees(float v) {
  impl_->occlusionTextureUvRotationDegrees_ =
      std::isfinite(v) ? std::clamp(v, -360.0f, 360.0f) : 0.0f;
}
float Material::occlusionTextureUvRotationDegrees() const {
  return impl_->occlusionTextureUvRotationDegrees_;
}

// Texture paths
void Material::setBaseColorTexture(const UniString& p) { impl_->baseColorTexture_ = p; impl_->hasBaseColorTexture_ = !p.toQString().trimmed().isEmpty(); }
UniString Material::baseColorTexture() const { return impl_->baseColorTexture_; }
bool Material::hasBaseColorTexture() const { return impl_->hasBaseColorTexture_; }
void Material::setMetallicRoughnessTexture(const UniString& p) { impl_->metallicRoughnessTexture_ = p; impl_->hasMetallicRoughnessTexture_ = !p.toQString().trimmed().isEmpty(); }
UniString Material::metallicRoughnessTexture() const { return impl_->metallicRoughnessTexture_; }
bool Material::hasMetallicRoughnessTexture() const { return impl_->hasMetallicRoughnessTexture_; }
void Material::setNormalTexture(const UniString& p) { impl_->normalTexture_ = p; impl_->hasNormalTexture_ = !p.toQString().trimmed().isEmpty(); }
UniString Material::normalTexture() const { return impl_->normalTexture_; }
bool Material::hasNormalTexture() const { return impl_->hasNormalTexture_; }
void Material::setEmissionTexture(const UniString& p) { impl_->emissionTexture_ = p; impl_->hasEmissionTexture_ = !p.toQString().trimmed().isEmpty(); }
UniString Material::emissionTexture() const { return impl_->emissionTexture_; }
bool Material::hasEmissionTexture() const { return impl_->hasEmissionTexture_; }
void Material::setOcclusionTexture(const UniString& p) { impl_->occlusionTexture_ = p; impl_->hasOcclusionTexture_ = !p.toQString().trimmed().isEmpty(); }
UniString Material::occlusionTexture() const { return impl_->occlusionTexture_; }
bool Material::hasOcclusionTexture() const { return impl_->hasOcclusionTexture_; }
void Material::setOpacityTexture(const UniString& p) { impl_->opacityTexture_ = p; impl_->hasOpacityTexture_ = !p.toQString().trimmed().isEmpty(); }
UniString Material::opacityTexture() const { return impl_->opacityTexture_; }
bool Material::hasOpacityTexture() const { return impl_->hasOpacityTexture_; }

// MaterialX
void Material::setMaterialXDocument(const UniString& xml) { impl_->materialXDocument_ = xml; }
UniString Material::materialXDocument() const { return impl_->materialXDocument_; }
bool Material::saveMaterialXDocument(const QString& filePath) const {
 if (filePath.trimmed().isEmpty()) return false;
 QFile file(filePath);
 if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
 const QByteArray data = materialXDocument().toQString().toUtf8();
 return file.write(data) == data.size();
}
bool Material::loadMaterialXDocument(const QString& filePath) {
  if (filePath.trimmed().isEmpty()) return false;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
  setMaterialXDocument(UniString::fromQString(QString::fromUtf8(file.readAll())));
  setType(MaterialType::MaterialX);
  return true;
}

// Material graph JSON
void Material::setMaterialGraphJson(const UniString& json) { impl_->materialGraphJson_ = json; }
UniString Material::materialGraphJson() const { return impl_->materialGraphJson_; }
bool Material::hasMaterialGraphJson() const { return !impl_->materialGraphJson_.toQString().trimmed().isEmpty(); }

// Presets
Material Material::makeDefault()
{
 Material m;
 m.setRoughness(0.5f);
 return m;
}

Material Material::makeMetal(const QColor& color)
{
 Material m;
 m.setBaseColor(color);
 m.setMetallic(1.0f);
 m.setRoughness(0.2f);
 return m;
}

Material Material::makePlastic(const QColor& color)
{
 Material m;
 m.setBaseColor(color);
 m.setMetallic(0.0f);
 m.setRoughness(0.4f);
 return m;
}

Material Material::makeGlass(const QColor& color)
{
 Material m;
 m.setBaseColor(color);
 m.setMetallic(0.0f);
 m.setRoughness(0.0f);
 m.setOpacity(0.2f);
 return m;
}

Material Material::makeEmissive(const QColor& color, float strength)
{
 Material m;
 m.setBaseColor(QColor(0, 0, 0));
 m.setEmissionColor(color);
 m.setEmissionStrength(strength);
 m.setRoughness(1.0f);
 return m;
}

Material Material::makeStudioPreset(MaterialPresetKind preset)
{
 Material material;
 switch (preset) {
 case MaterialPresetKind::Matte:
  material.setBaseColor(QColor(180, 180, 180));
  material.setMetallic(0.0f);
  material.setRoughness(0.82f);
  material.setSpecular(0.35f);
  material.setTransmission(0.0f);
  material.setIOR(1.45f);
  break;
 case MaterialPresetKind::Metal:
  material.setBaseColor(QColor(170, 180, 195));
  material.setMetallic(0.9f);
  material.setRoughness(0.24f);
  material.setSpecular(0.5f);
  material.setTransmission(0.0f);
  material.setIOR(1.45f);
  break;
 case MaterialPresetKind::Plastic:
  material.setBaseColor(QColor(210, 220, 235));
  material.setMetallic(0.0f);
  material.setRoughness(0.3f);
  material.setSpecular(0.5f);
  material.setTransmission(0.0f);
  material.setIOR(1.46f);
  break;
 case MaterialPresetKind::Glass:
  material.setBaseColor(QColor(225, 240, 255));
  material.setMetallic(0.0f);
  material.setRoughness(0.08f);
  material.setSpecular(0.5f);
  material.setTransmission(0.82f);
  material.setIOR(1.5f);
  break;
 }
 return material;
}

}
