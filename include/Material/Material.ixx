module;
#include <memory>
#include <QColor>
#include <QString>
export module Material.Material;

import Utils.String.UniString;

export namespace ArtifactCore {

// マテリアルタイプ
export enum class MaterialType {
    Standard,
    PBR,
    Unlit,
    MaterialX  // 将来のMaterialXノードグラフ用
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

    // 基本プロパティ
    void setName(const UniString& name);
    UniString name() const;
    void setType(MaterialType type);
    MaterialType type() const;
    void setBaseColor(const QColor& color);
    QColor baseColor() const;
    void setBaseColorTexture(const UniString& path);
    UniString baseColorTexture() const;
    // ... metallic, roughness, normal, emission, etc.

    // MaterialX拡張用
    void setMaterialXDocument(const UniString& xml);
    UniString materialXDocument() const;
};

}
