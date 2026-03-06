module;

#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

module Mesh;

import std;

namespace ArtifactCore {

    class Mesh::Impl {
    public:
        AttributeContainer vertexAttrs;
        AttributeContainer faceAttrs;
        AttributeContainer faceVertexAttrs;

        QVector<QVector<int>> polygons; // Face -> list of vertex indices

        QVector3D minBounds;
        QVector3D maxBounds;

        Impl() {}
        ~Impl() {}
    };

    Mesh::Mesh() : impl_(new Impl()) {}
    
    Mesh::Mesh(const Mesh& other) : impl_(new Impl(*other.impl_)) {}
    
    Mesh::Mesh(Mesh&& other) noexcept : impl_(other.impl_) {
        other.impl_ = nullptr;
    }
    
    Mesh::~Mesh() {
        delete impl_;
    }

    Mesh& Mesh::operator=(const Mesh& other) {
        if (this != &other) {
            *impl_ = *other.impl_;
        }
        return *this;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this != &other) {
            delete impl_;
            impl_ = other.impl_;
            other.impl_ = nullptr;
        }
        return *this;
    }

    void Mesh::setVertexCount(int count) {
        impl_->vertexAttrs.setElementCount(count);
    }

    int Mesh::vertexCount() const {
        return impl_->vertexAttrs.elementCount();
    }

    int Mesh::addPolygon(const QVector<int>& vertexIndices) {
        impl_->polygons.push_back(vertexIndices);
        impl_->faceAttrs.setElementCount(impl_->polygons.size());
        return impl_->polygons.size() - 1;
    }

    int Mesh::polygonCount() const {
        return impl_->polygons.size();
    }

    AttributeContainer& Mesh::vertexAttributes() { return impl_->vertexAttrs; }
    const AttributeContainer& Mesh::vertexAttributes() const { return impl_->vertexAttrs; }

    AttributeContainer& Mesh::faceAttributes() { return impl_->faceAttrs; }
    const AttributeContainer& Mesh::faceAttributes() const { return impl_->faceAttrs; }

    AttributeContainer& Mesh::faceVertexAttributes() { return impl_->faceVertexAttrs; }
    const AttributeContainer& Mesh::faceVertexAttributes() const { return impl_->faceVertexAttrs; }

    QVector<int> Mesh::getConnectedPolygons(int vertexIndex) const {
        QVector<int> result;
        for (int i = 0; i < impl_->polygons.size(); ++i) {
            if (impl_->polygons[i].contains(vertexIndex)) {
                result.push_back(i);
            }
        }
        return result;
    }

    QVector<int> Mesh::getPolygonVertices(int polygonIndex) const {
        if (polygonIndex >= 0 && polygonIndex < impl_->polygons.size()) {
            return impl_->polygons[polygonIndex];
        }
        return QVector<int>();
    }

    std::shared_ptr<Mesh> Mesh::createSubdivided(int level) const {
        // スタブ実装
        return std::make_shared<Mesh>(*this);
    }

    Mesh::RenderData Mesh::generateRenderData() const {
        RenderData data;
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        auto uvAttr = impl_->vertexAttrs.get<QVector2D>("uv");

        if (!posAttr) return data;

        // 簡単なTriangulate（ファン分割）
        for (int i = 0; i < impl_->polygons.size(); ++i) {
            const auto& poly = impl_->polygons[i];
            if (poly.size() < 3) continue;

            int v0 = poly[0];
            for (int j = 1; j < poly.size() - 1; ++j) {
                int v1 = poly[j];
                int v2 = poly[j + 1];

                // 頂点の展開 (Flat化)
                data.indices.push_back(data.positions.size());
                data.positions.push_back((*posAttr)[v0]);
                if (normAttr) data.normals.push_back((*normAttr)[v0]);
                if (uvAttr) data.uvs.push_back((*uvAttr)[v0]);

                data.indices.push_back(data.positions.size());
                data.positions.push_back((*posAttr)[v1]);
                if (normAttr) data.normals.push_back((*normAttr)[v1]);
                if (uvAttr) data.uvs.push_back((*uvAttr)[v1]);

                data.indices.push_back(data.positions.size());
                data.positions.push_back((*posAttr)[v2]);
                if (normAttr) data.normals.push_back((*normAttr)[v2]);
                if (uvAttr) data.uvs.push_back((*uvAttr)[v2]);
            }
        }

        return data;
    }

    void Mesh::applySkinning(const QVector<QMatrix4x4>& boneMatrices) {
        // スタブ実装
    }

    void Mesh::updateBounds() {
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        if (!posAttr || posAttr->size() == 0) return;

        QVector3D minB = (*posAttr)[0];
        QVector3D maxB = (*posAttr)[0];

        for (int i = 1; i < posAttr->size(); ++i) {
            const auto& p = (*posAttr)[i];
            minB.setX(std::min(minB.x(), p.x()));
            minB.setY(std::min(minB.y(), p.y()));
            minB.setZ(std::min(minB.z(), p.z()));
            
            maxB.setX(std::max(maxB.x(), p.x()));
            maxB.setY(std::max(maxB.y(), p.y()));
            maxB.setZ(std::max(maxB.z(), p.z()));
        }

        impl_->minBounds = minB;
        impl_->maxBounds = maxB;
    }

    QVector3D Mesh::boundingBoxMin() const { return impl_->minBounds; }
    QVector3D Mesh::boundingBoxMax() const { return impl_->maxBounds; }

    bool Mesh::loadFromFile(const QString& filePath) { return false; }
    bool Mesh::saveToFile(const QString& filePath) const { return false; }

    void Mesh::clear() {
        impl_->vertexAttrs.setElementCount(0);
        impl_->faceAttrs.setElementCount(0);
        impl_->faceVertexAttrs.setElementCount(0);
        impl_->polygons.clear();
    }

    bool Mesh::isValid() const {
        return impl_->vertexAttrs.elementCount() > 0 && impl_->polygons.size() > 0;
    }

}
