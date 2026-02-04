module;
#include <QString>
#include <QColor>
#include <utility>

module Material.Material;

import Utils.String.UniString;

namespace ArtifactCore {

class Material::Impl {
public:
    UniString name_;
    MaterialType type_ = MaterialType::Standard;
    QColor baseColor_ = QColor(255,255,255);
    UniString baseColorTexture_;
    UniString materialXDocument_;
};

Material::Material() : impl_(new Impl()) {}
Material::Material(const Material& other) : impl_(new Impl(*other.impl_)) {}
Material::Material(Material&& other) noexcept : impl_(other.impl_) { other.impl_ = nullptr; }
Material::~Material() { delete impl_; }
Material& Material::operator=(const Material& other) { if(this!=&other){ *impl_ = *other.impl_; } return *this; }
Material& Material::operator=(Material&& other) noexcept { if(this!=&other){ delete impl_; impl_ = other.impl_; other.impl_ = nullptr; } return *this; }

void Material::setName(const UniString& name) { impl_->name_ = name; }
UniString Material::name() const { return impl_->name_; }
void Material::setType(MaterialType type) { impl_->type_ = type; }
MaterialType Material::type() const { return impl_->type_; }
void Material::setBaseColor(const QColor& color) { impl_->baseColor_ = color; }
QColor Material::baseColor() const { return impl_->baseColor_; }
void Material::setBaseColorTexture(const UniString& path) { impl_->baseColorTexture_ = path; }
UniString Material::baseColorTexture() const { return impl_->baseColorTexture_; }
void Material::setMaterialXDocument(const UniString& xml) { impl_->materialXDocument_ = xml; }
UniString Material::materialXDocument() const { return impl_->materialXDocument_; }

} // namespace ArtifactCore
