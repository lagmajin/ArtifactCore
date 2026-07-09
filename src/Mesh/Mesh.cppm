module;
class tst_QList;

#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Mesh;

namespace ArtifactCore {

namespace {
constexpr int kTriangleIndexCount = 3;

int resolveObjIndex(const QString& token, const int elementCount)
{
    bool ok = false;
    int index = token.toInt(&ok);
    if (!ok || index == 0) {
        return -1;
    }

    if (index < 0) {
        index = elementCount + index;
    } else {
        index -= 1;
    }

    return (index >= 0 && index < elementCount) ? index : -1;
}

Mesh::Meshlet buildMeshletFromIndexRange(const Mesh::RenderData& renderData,
                                         const QVector<unsigned int>& indices,
                                         const int firstIndex,
                                         const int indexCount,
                                         const int triangleStride)
{
    Mesh::Meshlet meshlet;
    meshlet.firstIndex = static_cast<unsigned int>(firstIndex);
    meshlet.indexCount = static_cast<unsigned int>(indexCount);
    meshlet.sourceTriangleCount = (indexCount / kTriangleIndexCount) * triangleStride;

    QVector3D boundsMin(std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max());
    QVector3D boundsMax(std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest());

    bool hasVertex = false;
    const int endIndex = firstIndex + indexCount;
    for (int indexOffset = firstIndex; indexOffset < endIndex; ++indexOffset) {
        if (indexOffset < 0 || indexOffset >= indices.size()) {
            continue;
        }

        const unsigned int vertexIndex = indices[indexOffset];
        if (vertexIndex >= static_cast<unsigned int>(renderData.positions.size())) {
            continue;
        }

        const QVector3D& p = renderData.positions[static_cast<int>(vertexIndex)];
        boundsMin.setX(std::min(boundsMin.x(), p.x()));
        boundsMin.setY(std::min(boundsMin.y(), p.y()));
        boundsMin.setZ(std::min(boundsMin.z(), p.z()));
        boundsMax.setX(std::max(boundsMax.x(), p.x()));
        boundsMax.setY(std::max(boundsMax.y(), p.y()));
        boundsMax.setZ(std::max(boundsMax.z(), p.z()));
        hasVertex = true;
    }

    if (!hasVertex) {
        return meshlet;
    }

    meshlet.boundsMin = boundsMin;
    meshlet.boundsMax = boundsMax;
    meshlet.boundsCenter = (boundsMin + boundsMax) * 0.5f;
    meshlet.boundsRadius = 0.0f;
    for (int indexOffset = firstIndex; indexOffset < endIndex; ++indexOffset) {
        if (indexOffset < 0 || indexOffset >= indices.size()) {
            continue;
        }

        const unsigned int vertexIndex = indices[indexOffset];
        if (vertexIndex >= static_cast<unsigned int>(renderData.positions.size())) {
            continue;
        }

        meshlet.boundsRadius = std::max(
            meshlet.boundsRadius,
            (renderData.positions[static_cast<int>(vertexIndex)] - meshlet.boundsCenter).length());
    }

    return meshlet;
}
}

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

    // スキニング用のウェイト構造体 (最大4ボーン)
    struct BoneWeight {
        int boneIndices[4] = {0, 0, 0, 0};
        float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    std::shared_ptr<Mesh> Mesh::createSubdivided(int level) const {
        if (level <= 0 || impl_->polygons.empty()) return std::make_shared<Mesh>(*this);

        auto subdivided = std::make_shared<Mesh>();
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        if (!posAttr) return subdivided;

        auto newPosAttr = subdivided->vertexAttributes().add<QVector3D>("position");
        auto newNormAttr = subdivided->vertexAttributes().add<QVector3D>("normal");

        int originalVertexCount = impl_->vertexAttrs.elementCount();
        int faceCount = impl_->polygons.size();
        
        // 元の頂点をコピー
        subdivided->setVertexCount(originalVertexCount);
        for (int i = 0; i < originalVertexCount; ++i) {
            (*newPosAttr)[i] = (*posAttr)[i];
            if (normAttr) (*newNormAttr)[i] = (*normAttr)[i];
        }

        // 簡易 Catmull-Clark (Linear Subdivision): 各ポリゴンをN個の四角形に分割
        // 1. 各面の中点 (Face Point) を計算して追加
        QVector<int> facePointIndices;
        facePointIndices.resize(faceCount);
        for (int i = 0; i < faceCount; ++i) {
            const auto& poly = impl_->polygons[i];
            QVector3D faceCenter(0, 0, 0);
            QVector3D faceNormal(0, 0, 0);
            for (int vIdx : poly) {
                faceCenter += (*posAttr)[vIdx];
                if (normAttr) faceNormal += (*normAttr)[vIdx];
            }
            faceCenter /= std::max<int>(1, (int)poly.size());
            if (normAttr) faceNormal.normalize();

            int fpIdx = subdivided->vertexCount();
            subdivided->setVertexCount(fpIdx + 1);
            (*newPosAttr)[fpIdx] = faceCenter;
            if (normAttr) (*newNormAttr)[fpIdx] = faceNormal;
            facePointIndices[i] = fpIdx;
        }

        // 2. 新しいポリゴン(Quad)を構築
        for (int i = 0; i < faceCount; ++i) {
            const auto& poly = impl_->polygons[i];
            int fpIdx = facePointIndices[i];
            int n = poly.size();

            for (int j = 0; j < n; ++j) {
                int v0 = poly[j];
                int v1 = poly[(j + 1) % n];
                int v_prev = poly[(j - 1 + n) % n];

                // エッジの中点 (簡易版: 共有エッジを考慮せず独自に生成)
                QVector3D edgeCenter1 = ((*posAttr)[v0] + (*posAttr)[v1]) * 0.5f;
                QVector3D edgeCenter2 = ((*posAttr)[v0] + (*posAttr)[v_prev]) * 0.5f;

                int epIdx1 = subdivided->vertexCount();
                subdivided->setVertexCount(epIdx1 + 2);
                (*newPosAttr)[epIdx1] = edgeCenter1;
                (*newPosAttr)[epIdx1 + 1] = edgeCenter2;

                if (normAttr) {
                    (*newNormAttr)[epIdx1] = ((*normAttr)[v0] + (*normAttr)[v1]).normalized();
                    (*newNormAttr)[epIdx1 + 1] = ((*normAttr)[v0] + (*normAttr)[v_prev]).normalized();
                }

                // 新しい四角形: [元の頂点, 次のエッジ中点, 面の中点, 前のエッジ中点]
                subdivided->addPolygon({v0, epIdx1, fpIdx, epIdx1 + 1});
            }
        }

        // レベルが2以上の場合は再帰的に分割
        if (level > 1) {
            return subdivided->createSubdivided(level - 1);
        }
        
        subdivided->updateBounds();
        return subdivided;
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

    Mesh::MeshletLODData Mesh::generateMeshletLODData(const MeshletLODConfig& config) const
    {
        MeshletLODData data;
        data.renderData = generateRenderData();
        const int triangleCount = data.renderData.indices.size() / kTriangleIndexCount;
        if (triangleCount <= 0 || data.renderData.positions.isEmpty()) {
            return data;
        }

        const int maxTrianglesPerMeshlet = std::max(1, config.maxTrianglesPerMeshlet);
        const int maxLODLevels = std::max(1, config.maxLODLevels);
        int triangleStride = 1;

        for (int level = 0; level < maxLODLevels; ++level) {
            const int levelOffset = data.meshlets.size();
            const int levelFirstIndex = data.lodIndices.size();
            const int trianglesPerMeshlet = maxTrianglesPerMeshlet;

            for (int triangle = 0; triangle < triangleCount; triangle += triangleStride) {
                const int sourceIndex = triangle * kTriangleIndexCount;
                data.lodIndices.push_back(data.renderData.indices[sourceIndex + 0]);
                data.lodIndices.push_back(data.renderData.indices[sourceIndex + 1]);
                data.lodIndices.push_back(data.renderData.indices[sourceIndex + 2]);
            }

            const int levelIndexCount = data.lodIndices.size() - levelFirstIndex;
            const int meshletIndexCount = trianglesPerMeshlet * kTriangleIndexCount;
            for (int firstIndex = levelFirstIndex; firstIndex < levelFirstIndex + levelIndexCount; firstIndex += meshletIndexCount) {
                const int remainingIndices = (levelFirstIndex + levelIndexCount) - firstIndex;
                const int indexCount = std::min(meshletIndexCount, remainingIndices);
                data.meshlets.push_back(
                    buildMeshletFromIndexRange(data.renderData, data.lodIndices, firstIndex, indexCount, triangleStride));
            }

            MeshletLODLevel lodLevel;
            lodLevel.level = level;
            lodLevel.meshletOffset = levelOffset;
            lodLevel.meshletCount = data.meshlets.size() - levelOffset;
            lodLevel.firstIndex = static_cast<unsigned int>(levelFirstIndex);
            lodLevel.indexCount = static_cast<unsigned int>(levelIndexCount);
            lodLevel.triangleStride = triangleStride;
            lodLevel.switchDistancePixels = config.lodSwitchPixels / static_cast<float>(triangleStride);
            data.levels.push_back(lodLevel);

            if (triangleStride >= triangleCount) {
                break;
            }

            triangleStride *= 2;
        }

        return data;
    }

    int Mesh::chooseMeshletLODLevel(const MeshletLODData& data, const float projectedRadiusPixels)
    {
        if (data.levels.isEmpty()) {
            return 0;
        }

        int selectedLevel = data.levels.front().level;
        for (const MeshletLODLevel& level : data.levels) {
            if (projectedRadiusPixels <= level.switchDistancePixels) {
                selectedLevel = level.level;
            }
        }

        return selectedLevel;
    }

    void Mesh::applySkinning(const QVector<QMatrix4x4>& boneMatrices) {
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        // "boneWeights" アトリビュートを検索
        auto weightAttr = impl_->vertexAttrs.get<BoneWeight>("boneWeights");

        if (!posAttr || !weightAttr || boneMatrices.isEmpty()) return;

        // Linear Blend Skinning (LBS) の評価
        for (int i = 0; i < impl_->vertexAttrs.elementCount(); ++i) {
            const auto& bw = (*weightAttr)[i];
            QVector3D originalPos = (*posAttr)[i];
            QVector3D originalNorm = normAttr ? (*normAttr)[i] : QVector3D(0,1,0);

            QVector3D skinnedPos(0, 0, 0);
            QVector3D skinnedNorm(0, 0, 0);

            float totalWeight = 0.0f;

            for (int j = 0; j < 4; ++j) {
                float w = bw.weights[j];
                if (w > 0.0f) {
                    int bIdx = bw.boneIndices[j];
                    if (bIdx >= 0 && bIdx < boneMatrices.size()) {
                        const QMatrix4x4& mat = boneMatrices[bIdx];
                        skinnedPos += mat.map(originalPos) * w;
                        // 法線は回転成分のみを適用
                        skinnedNorm += mat.mapVector(originalNorm) * w;
                        totalWeight += w;
                    }
                }
            }

            if (totalWeight > 0.0f) {
                (*posAttr)[i] = skinnedPos;
                if (normAttr) (*normAttr)[i] = skinnedNorm.normalized();
            }
        }
        
        updateBounds();
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

    bool Mesh::loadFromFile(const QString& filePath)
    {
        const QString trimmed = filePath.trimmed();
        if (trimmed.isEmpty()) {
            return false;
        }

        QFile file(trimmed);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }

        QVector<QVector3D> positions;
        QVector<QVector3D> normals;
        QVector<QVector2D> texCoords;
        QVector<QVector<int>> polygons;

        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) {
                continue;
            }

            const QStringList tokens = line.split(QChar::Space, Qt::SkipEmptyParts);
            if (tokens.isEmpty()) {
                continue;
            }

            const QString& tag = tokens.front();
            if (tag == QStringLiteral("v") && tokens.size() >= 4) {
                bool okX = false, okY = false, okZ = false;
                const float x = tokens[1].toFloat(&okX);
                const float y = tokens[2].toFloat(&okY);
                const float z = tokens[3].toFloat(&okZ);
                if (okX && okY && okZ) {
                    positions.append(QVector3D(x, y, z));
                }
            } else if (tag == QStringLiteral("vn") && tokens.size() >= 4) {
                bool okX = false, okY = false, okZ = false;
                const float x = tokens[1].toFloat(&okX);
                const float y = tokens[2].toFloat(&okY);
                const float z = tokens[3].toFloat(&okZ);
                if (okX && okY && okZ) {
                    normals.append(QVector3D(x, y, z));
                }
            } else if (tag == QStringLiteral("vt") && tokens.size() >= 3) {
                bool okU = false, okV = false;
                const float u = tokens[1].toFloat(&okU);
                const float v = tokens[2].toFloat(&okV);
                if (okU && okV) {
                    texCoords.append(QVector2D(u, v));
                }
            } else if (tag == QStringLiteral("f") && tokens.size() >= 4) {
                QVector<int> polygon;
                polygon.reserve(tokens.size() - 1);
                for (int i = 1; i < tokens.size(); ++i) {
                    const QStringList vertexParts = tokens[i].split(QChar('/'));
                    const int posIndex = resolveObjIndex(vertexParts.value(0), positions.size());
                    if (posIndex >= 0) {
                        polygon.append(posIndex);
                    }
                }
                if (polygon.size() >= 3) {
                    polygons.append(std::move(polygon));
                }
            }
        }

        if (positions.isEmpty() || polygons.isEmpty()) {
            clear();
            return false;
        }

        clear();
        setVertexCount(positions.size());

        auto posAttr = vertexAttributes().add<QVector3D>("position");
        for (int i = 0; i < positions.size(); ++i) {
            (*posAttr)[i] = positions[i];
        }

        if (normals.size() == positions.size()) {
            auto normAttr = vertexAttributes().add<QVector3D>("normal");
            for (int i = 0; i < normals.size(); ++i) {
                (*normAttr)[i] = normals[i];
            }
        }

        if (texCoords.size() == positions.size()) {
            auto uvAttr = vertexAttributes().add<QVector2D>("uv");
            for (int i = 0; i < texCoords.size(); ++i) {
                (*uvAttr)[i] = texCoords[i];
            }
        }

        for (const auto& polygon : polygons) {
            addPolygon(polygon);
        }

        updateBounds();
        return true;
    }

    bool Mesh::saveToFile(const QString& filePath) const
    {
        const QString trimmed = filePath.trimmed();
        if (trimmed.isEmpty() || !isValid()) {
            return false;
        }

        QFile file(trimmed);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }

        const auto posAttr = vertexAttributes().get<QVector3D>("position");
        if (!posAttr || posAttr->size() == 0) {
            return false;
        }

        const auto normAttr = vertexAttributes().get<QVector3D>("normal");
        const auto uvAttr = vertexAttributes().get<QVector2D>("uv");

        QTextStream out(&file);
        out << "# ArtifactCore Mesh OBJ export\n";
        for (int i = 0; i < posAttr->size(); ++i) {
            const QVector3D& p = (*posAttr)[i];
            out << "v " << p.x() << ' ' << p.y() << ' ' << p.z() << '\n';
        }

        if (normAttr && normAttr->size() == posAttr->size()) {
            for (int i = 0; i < normAttr->size(); ++i) {
                const QVector3D& n = (*normAttr)[i];
                out << "vn " << n.x() << ' ' << n.y() << ' ' << n.z() << '\n';
            }
        }

        if (uvAttr && uvAttr->size() == posAttr->size()) {
            for (int i = 0; i < uvAttr->size(); ++i) {
                const QVector2D& uv = (*uvAttr)[i];
                out << "vt " << uv.x() << ' ' << uv.y() << '\n';
            }
        }

        for (const auto& polygon : impl_->polygons) {
            if (polygon.size() < 3) {
                continue;
            }
            out << "f";
            for (const int vertexIndex : polygon) {
                out << ' ' << (vertexIndex + 1);
            }
            out << '\n';
        }

        return out.status() == QTextStream::Ok;
    }

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
