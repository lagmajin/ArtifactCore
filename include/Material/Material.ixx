module;
#include <memory>
#include <QColor>
#include <QString>
export module Material.Material;

import Utils.String.UniString;

export namespace ArtifactCore {

// �}�e���A���^�C�v
export enum class MaterialType {
    Standard,
    PBR,
    Unlit,
    MaterialX  // ������MaterialX�m�[�h�O���t�p
};

export class Material {
private:
    class Impl;
    Impl* impl_;
public:
    Material();
    Material(const Material& other);
    Material(Material&& other) noexcept;
    ~Material();
    Material& operator=(const Material& other);
    Material& operator=(Material&& other) noexcept;

    // ��{�v���p�e�B
    void setName(const UniString& name);
    UniString name() const;
    void setType(MaterialType type);
    MaterialType type() const;
    void setBaseColor(const QColor& color);
    QColor baseColor() const;
    void setBaseColorTexture(const UniString& path);
    UniString baseColorTexture() const;
    // ... metallic, roughness, normal, emission, etc.

    // MaterialX�g���p
    void setMaterialXDocument(const UniString& xml);
    UniString materialXDocument() const;
};

}
