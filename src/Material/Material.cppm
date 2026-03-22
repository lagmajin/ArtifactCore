module;
#include <QString>
#include <QColor>
#include <QVector3D>
#include <utility>
#include <algorithm>

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

 // Emission
 QColor emissionColor_ = QColor(0, 0, 0);
 float emissionStrength_ = 0.0f;

 // Opacity
 float opacity_ = 1.0f;

 // Normal
 float normalStrength_ = 1.0f;

 // AO
 float occlusionStrength_ = 1.0f;

 // Texture paths
 UniString baseColorTexture_;
 UniString metallicRoughnessTexture_;
 UniString normalTexture_;
 UniString emissionTexture_;
 UniString occlusionTexture_;
 UniString opacityTexture_;

 // MaterialX
 UniString materialXDocument_;
};

Material::Material() : impl_(new Impl()) {}
Material::Material(MaterialType type) : impl_(new Impl()) { impl_->type_ = type; }
Material::Material(const Material& other) : impl_(new Impl(*other.impl_)) {}
Material::Material(Material&& other) noexcept : impl_(other.impl_) { other.impl_ = nullptr; }
Material::~Material() { delete impl_; }
Material& Material::operator=(const Material& other) { if(this!=&other){ *impl_ = *other.impl_; } return *this; }
Material& Material::operator=(Material&& other) noexcept { if(this!=&other){ delete impl_; impl_ = other.impl_; other.impl_ = nullptr; } return *this; }

// Identity
void Material::setName(const UniString& name) { impl_->name_ = name; }
UniString Material::name() const { return impl_->name_; }
void Material::setType(MaterialType type) { impl_->type_ = type; }
MaterialType Material::type() const { return impl_->type_; }

// PBR Base
void Material::setBaseColor(const QColor& color) { impl_->baseColor_ = color; }
QColor Material::baseColor() const { return impl_->baseColor_; }
void Material::setMetallic(float v) { impl_->metallic_ = std::clamp(v, 0.0f, 1.0f); }
float Material::metallic() const { return impl_->metallic_; }
void Material::setRoughness(float v) { impl_->roughness_ = std::clamp(v, 0.0f, 1.0f); }
float Material::roughness() const { return impl_->roughness_; }

// Emission
void Material::setEmissionColor(const QColor& color) { impl_->emissionColor_ = color; }
QColor Material::emissionColor() const { return impl_->emissionColor_; }
void Material::setEmissionStrength(float v) { impl_->emissionStrength_ = std::max(v, 0.0f); }
float Material::emissionStrength() const { return impl_->emissionStrength_; }

// Opacity
void Material::setOpacity(float v) { impl_->opacity_ = std::clamp(v, 0.0f, 1.0f); }
float Material::opacity() const { return impl_->opacity_; }

// Normal
void Material::setNormalStrength(float v) { impl_->normalStrength_ = std::max(v, 0.0f); }
float Material::normalStrength() const { return impl_->normalStrength_; }

// AO
void Material::setOcclusionStrength(float v) { impl_->occlusionStrength_ = std::clamp(v, 0.0f, 1.0f); }
float Material::occlusionStrength() const { return impl_->occlusionStrength_; }

// Texture paths
void Material::setBaseColorTexture(const UniString& p) { impl_->baseColorTexture_ = p; }
UniString Material::baseColorTexture() const { return impl_->baseColorTexture_; }
void Material::setMetallicRoughnessTexture(const UniString& p) { impl_->metallicRoughnessTexture_ = p; }
UniString Material::metallicRoughnessTexture() const { return impl_->metallicRoughnessTexture_; }
void Material::setNormalTexture(const UniString& p) { impl_->normalTexture_ = p; }
UniString Material::normalTexture() const { return impl_->normalTexture_; }
void Material::setEmissionTexture(const UniString& p) { impl_->emissionTexture_ = p; }
UniString Material::emissionTexture() const { return impl_->emissionTexture_; }
void Material::setOcclusionTexture(const UniString& p) { impl_->occlusionTexture_ = p; }
UniString Material::occlusionTexture() const { return impl_->occlusionTexture_; }
void Material::setOpacityTexture(const UniString& p) { impl_->opacityTexture_ = p; }
UniString Material::opacityTexture() const { return impl_->opacityTexture_; }

// MaterialX
void Material::setMaterialXDocument(const UniString& xml) { impl_->materialXDocument_ = xml; }
UniString Material::materialXDocument() const { return impl_->materialXDocument_; }

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

}
