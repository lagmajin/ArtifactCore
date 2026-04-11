module;
#include <QColor>
#include <QString>

module Material.Material3D;

import Utils.String.UniString;

import Utils.String.UniString;

namespace ArtifactCore {

class Material3D::Impl {
public:
  UniString name_;
  QColor diffuseColor_ = QColor(200, 200, 200);
  UniString diffuseTexture_;
  QColor specularColor_ = QColor(255, 255, 255);
  float specularStrength_ = 0.5f;
  UniString specularTexture_;
  float roughness_ = 0.5f;
  UniString roughnessTexture_;
  float normalStrength_ = 1.0f;
  UniString normalTexture_;
  UniString materialXDocument_;
};

Material3D::Material3D() : impl_(new Impl()) {}
Material3D::Material3D(const Material3D &other)
    : impl_(new Impl(*other.impl_)) {}
Material3D::Material3D(Material3D &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}
Material3D::~Material3D() { delete impl_; }

Material3D &Material3D::operator=(const Material3D &other) {
  if (this != &other) {
    *impl_ = *other.impl_;
  }
  return *this;
}

Material3D &Material3D::operator=(Material3D &&other) noexcept {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

// --- Identity ---
void Material3D::setName(const UniString &name) { impl_->name_ = name; }
UniString Material3D::name() const { return impl_->name_; }

// --- Diffuse ---
void Material3D::setDiffuseColor(const QColor &color) {
  impl_->diffuseColor_ = color;
}
QColor Material3D::diffuseColor() const { return impl_->diffuseColor_; }
void Material3D::setDiffuseTexture(const UniString &path) {
  impl_->diffuseTexture_ = path;
}
UniString Material3D::diffuseTexture() const { return impl_->diffuseTexture_; }

// --- Specular ---
void Material3D::setSpecularColor(const QColor &color) {
  impl_->specularColor_ = color;
}
QColor Material3D::specularColor() const { return impl_->specularColor_; }
void Material3D::setSpecularStrength(float value) {
  impl_->specularStrength_ = value;
}
float Material3D::specularStrength() const { return impl_->specularStrength_; }
void Material3D::setSpecularTexture(const UniString &path) {
  impl_->specularTexture_ = path;
}
UniString Material3D::specularTexture() const {
  return impl_->specularTexture_;
}

// --- Roughness ---
void Material3D::setRoughness(float value) { impl_->roughness_ = value; }
float Material3D::roughness() const { return impl_->roughness_; }
void Material3D::setRoughnessTexture(const UniString &path) {
  impl_->roughnessTexture_ = path;
}
UniString Material3D::roughnessTexture() const {
  return impl_->roughnessTexture_;
}

// --- Normal ---
void Material3D::setNormalStrength(float value) {
  impl_->normalStrength_ = value;
}
float Material3D::normalStrength() const { return impl_->normalStrength_; }
void Material3D::setNormalTexture(const UniString &path) {
  impl_->normalTexture_ = path;
}
UniString Material3D::normalTexture() const { return impl_->normalTexture_; }

// --- Presets ---
Material3D Material3D::makeDefault() {
  Material3D mat;
  mat.setName(UniString("Default Material"));
  return mat;
}

Material3D Material3D::makePlastic(const QColor &color) {
  Material3D mat;
  mat.setName(UniString("Plastic Material"));
  mat.setDiffuseColor(color);
  mat.setSpecularColor(QColor(255, 255, 255));
  mat.setSpecularStrength(0.1f);
  mat.setRoughness(0.8f);
  return mat;
}

Material3D Material3D::makeMetal(const QColor &color) {
  Material3D mat;
  mat.setName(UniString("Metal Material"));
  mat.setDiffuseColor(color);
  mat.setSpecularColor(color);
  mat.setSpecularStrength(0.9f);
  mat.setRoughness(0.1f);
  return mat;
}

// --- MaterialX ---
void Material3D::setMaterialXDocument(const UniString &xml) {
  impl_->materialXDocument_ = xml;
}
UniString Material3D::materialXDocument() const {
  return impl_->materialXDocument_;
}
bool Material3D::hasMaterialXDocument() const {
  return !impl_->materialXDocument_.toQString().trimmed().isEmpty();
}
void Material3D::clearMaterialXDocument() {
  impl_->materialXDocument_ = UniString();
}

} // namespace ArtifactCore