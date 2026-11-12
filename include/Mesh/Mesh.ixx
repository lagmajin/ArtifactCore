module;
class tst_QList;

#include <memory>
#include <QString>
#include <QVector>
#include <QVariant>
#include <QHash>
#include <QVector3D>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QtCore/QObject>
#include <typeindex>
#include <unordered_map>
#include "../Define/DllExportMacro.hpp"

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
#include <cstdint>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Mesh;

import Memory.SharedPtr;
import Container.NamedVector;

export namespace ArtifactCore {

    // ─────────────────────────────────────────────────────────
    // 動的アトリビュートシステム (MayaのBlindDataやAttributeに相当)
    // 頂点、エッジ、フェースに対して、任意の型のデータを動的に追加・取得できる
    // ─────────────────────────────────────────────────────────
    class MeshAttributeBase {
    public:
        virtual ~MeshAttributeBase() = default;
        virtual std::type_index type() const = 0;
        virtual int size() const = 0;
        virtual void resize(int newSize) = 0;
    };

    template<typename T>
    class MeshAttribute : public MeshAttributeBase {
    private:
        QVector<T> data_;
    public:
        std::type_index type() const override { return typeid(T); }
        int size() const override { return data_.size(); }
        void resize(int newSize) override { data_.resize(newSize); }
        
        T& operator[](int index) { return data_[index]; }
        const T& operator[](int index) const { return data_[index]; }
        QVector<T>& data() { return data_; }
    };

    // ─────────────────────────────────────────────────────────
    // 要素（コンポーネント）ごとのアトリビュートコンテナ
    // ─────────────────────────────────────────────────────────
    class AttributeContainer {
    private:
        std::unordered_map<std::string, SharedPtr<MeshAttributeBase>> attributes_;
        int elementCount_ = 0;
    public:
        void setElementCount(int count) {
            elementCount_ = count;
            for (auto& pair : attributes_) {
                pair.second->resize(count);
            }
        }
        int elementCount() const { return elementCount_; }

        template<typename T>
        SharedPtr<MeshAttribute<T>> add(const std::string& name) {
            auto attr = makeShared<MeshAttribute<T>>();
            attr->resize(elementCount_);
            attributes_[name] = attr;
            return attr;
        }

        template<typename T>
        SharedPtr<MeshAttribute<T>> get(const std::string& name) const {
            auto it = attributes_.find(name);
            if (it != attributes_.end() && it->second->type() == typeid(T)) {
                return staticPointerCast<MeshAttribute<T>>(it->second);
            }
            return nullptr;
        }

        bool has(const std::string& name) const { return attributes_.count(name) > 0; }
        std::vector<std::string> attributeNames() const {
            NamedVector<std::string> names;
            for (const auto& pair : attributes_) names.append(pair.first);
            return names.toStdVector();
        }
    };

    // ─────────────────────────────────────────────────────────
    // プロ向け DDC メッシュクラス (N-gon & アトリビュート対応)
    // ─────────────────────────────────────────────────────────
    class LIBRARY_DLL_API Mesh {
    private:
        class Impl;
        Impl* impl_;

    public:
        enum class SkinningMethod {
            LinearBlend,             // skinMethod = 0
            Rigid,                   // skinMethod = 1
            DualQuaternion,          // skinMethod = 2
            BlendedDualQuaternion    // skinMethod = 3
        };

        struct SkinBone {
            QString name;
            int parentIndex = -1;
            QMatrix4x4 bindMatrix;
            QMatrix4x4 poseMatrix;
        };

        struct SkinAnimationClip {
            QString name;
            double timeBegin = 0.0;
            double timeEnd = 0.0;
        };

        struct BlendShape {
            QString name;
            float weight = 0.0f;
            QVector<QVector3D> positionOffsets;
            QVector<QVector3D> normalOffsets;
        };

        // Contiguous triangle/index range sharing one material. Importers
        // record the source polygon range so flattened render data can retain
        // per-triangle material identity.
        struct MaterialSlot {
            int firstIndex = 0;
            int indexCount = 0;
            int materialIndex = 0;
        };

        Mesh();
        Mesh(const Mesh& other);
        Mesh(Mesh&& other) noexcept;
        ~Mesh();

        Mesh& operator=(const Mesh& other);
        Mesh& operator=(Mesh&& other) noexcept;
        std::uint64_t revision() const;

        // 1. トポロジー構築 (N-gon対応)
        // Mayaのように、頂点のリストと、それらを結ぶ「面（ポリゴン）」のリストで構成
        void setVertexCount(int count);
        int vertexCount() const;

        // 面を追加 (3角形、4角形、N角形すべてを許容)
        // 戻り値は追加されたFaceのインデックス
        int addPolygon(const QVector<int>& vertexIndices);
        int polygonCount() const;

        // Material slot ranges (see MaterialSlot).  Empty means the whole mesh
        // uses a single material and the renderer may issue one draw.
        int addMaterialSlot(int firstIndex, int indexCount, int materialIndex);
        int materialSlotCount() const;
        const QVector<MaterialSlot>& materialSlots() const;
        void clearMaterialSlots();

        // 2. 動的アトリビュートへのアクセス
        // 固定の Vertex 構造体ではなく、名前でデータにアクセスする
        // 例: mesh.vertexAttributes().get<QVector3D>("position");
        AttributeContainer& vertexAttributes();
        const AttributeContainer& vertexAttributes() const;

        AttributeContainer& faceAttributes();
        const AttributeContainer& faceAttributes() const;

        // Face-Vertex (面を構成する頂点ごと) のアトリビュート。Mayaの「UV」などはここに入る
        AttributeContainer& faceVertexAttributes();
        const AttributeContainer& faceVertexAttributes() const;

        const QVector<SkinBone>& skinBones() const;
        void setSkinBones(const QVector<SkinBone>& bones);
        // Update the evaluated pose without replacing bind data or skin weights.
        bool setSkinPoseMatrices(const QVector<QMatrix4x4>& poseMatrices);
        SkinningMethod skinningMethod() const;
        void setSkinningMethod(SkinningMethod method);
        // True when CPU-only packed influences beyond the shader's first four exist.
        bool hasExtendedSkinningWeights() const;
        QVector<QMatrix4x4> skinPoseMatrices() const;
        void invalidateSkinningBase();
        // Restore the cached bind-space attributes after CPU skinning.
        void restoreSkinningBase();
        const QVector<SkinAnimationClip>& skinAnimationClips() const;
        const SkinAnimationClip* skinAnimationClip(int index) const;
        void setSkinAnimationClips(const QVector<SkinAnimationClip>& clips);
        const QVector<BlendShape>& blendShapes() const;
        void setBlendShapes(const QVector<BlendShape>& shapes);
        float blendShapeWeight(int index) const;
        void setBlendShapeWeight(int index, float weight);
        void applyBlendShapes();

        // 3. 高度なトポロジー参照 (Half-Edge相当のクエリ)
        // ある頂点に接続しているすべての面を取得する
        QVector<int> getConnectedPolygons(int vertexIndex) const;
        // ある面に含まれる頂点インデックスを取得する
        QVector<int> getPolygonVertices(int polygonIndex) const;

        // Explicit, CPU-side analysis of the original polygon indices. This
        // allocates temporary storage and must not be called per frame.
        struct TopologyStatistics {
            bool topologyAnalysisAvailable = true;
            int topologyVertexCount = 0;
            int invalidPolygonCount = 0; // fewer than 3 vertices, repeated or invalid indices
            int validPolygonCount = 0;
            std::int64_t edgeCount = 0;
            int boundaryEdgeCount = 0;
            int nonManifoldEdgeCount = 0; // more than two incident faces
            int inconsistentWindingEdgeCount = 0; // two faces traverse the edge alike
            int inconsistentWindingFaceCount = 0; // both faces incident to any such edge
            int duplicateFaceCount = 0; // copies after the first, either winding
            int isolatedVertexCount = 0;
            int connectedComponentCount = 0; // vertex-connected, includes isolated vertices
            std::int64_t eulerCharacteristic = 0; // V - E + F, including duplicate valid faces
            bool positionAnalysisAvailable = false;
            int degenerateTriangleCount = 0; // in-range fan triangles with area <= tolerance, including invalid polygons
            int nonFinitePositionTriangleCount = 0;
        };
        // Invalid polygons do not contribute to topology, so their vertices
        // alone are considered isolated. Winding is a local consistency check;
        // it cannot tell which face is inverted or detect a whole inverted shell.
        // areaTolerance is in squared model-space units (triangle area).
        // Exact-position welding is opt-in: it helps with split-vertex imports,
        // but can join separate shells. It requires all positions to be finite;
        // otherwise topologyAnalysisAvailable is false and no counts are valid.
        TopologyStatistics analyzeTopology(double areaTolerance = 0.0,
                                            bool weldExactPositions = false) const;

        struct SelfIntersectionStatistics {
            bool positionAnalysisAvailable = false;
            int invalidPolygonCount = 0;
            int skippedTriangleCount = 0; // non-finite or zero-area
            std::int64_t samePolygonPairCount = 0; // fan triangles from one polygon; not tested
            std::int64_t adjacentPairCount = 0; // shares a vertex; not tested
            std::int64_t intersectingPairCount = 0; // fan triangle pairs, includes touching
        };
        // Exhaustive O(T^2) diagnostic, separate from the cheaper topology
        // analysis. Same-polygon and shared-vertex pairs are excluded; the
        // latter can still overlap beyond their shared vertex. With
        // weldExactPositions, exactly coincident positions also count as
        // shared vertices; this can hide intersections between separate shells.
        // tolerance is a non-negative distance in model-space units.
        SelfIntersectionStatistics analyzeSelfIntersections(double tolerance = 0.0,
                                                             bool weldExactPositions = false) const;

        // 4. サブディビジョンと非破壊モディファイアの基盤
        // サブディビジョンサーフェス（Catmull-Clark等）を適用した新しいメッシュを生成
        SharedPtr<Mesh> createSubdivided(int level) const;
        
        // レンダリング用（GPU用）に、すべてを三角形に分割したフラットな配列を生成する
        struct RenderData {
            QVector<QVector3D> positions;
            QVector<QVector3D> normals;
            QVector<QVector4D> tangents;
            QVector<QVector2D> uvs;
            QVector<QVector2D> uvs1;
            QVector<QVector4D> colors;
            QVector<unsigned int> indices;
            // One imported material index per generated triangle; empty when
            // the mesh has no explicit material-slot ranges.
            QVector<unsigned int> triangleMaterialIndices;
        };
        RenderData generateRenderData() const;

        struct MeshletLODConfig {
            int maxTrianglesPerMeshlet = 64;
            int maxLODLevels = 4;
            float lodSwitchPixels = 96.0f;
        };

        struct Meshlet {
            unsigned int firstIndex = 0;
            unsigned int indexCount = 0;
            QVector3D boundsMin;
            QVector3D boundsMax;
            QVector3D boundsCenter;
            float boundsRadius = 0.0f;
            int sourceTriangleCount = 0;
        };

        struct MeshletLODLevel {
            int level = 0;
            int meshletOffset = 0;
            int meshletCount = 0;
            unsigned int firstIndex = 0;
            unsigned int indexCount = 0;
            int triangleStride = 1;
            float switchDistancePixels = 0.0f;
        };

        struct MeshletLODData {
            RenderData renderData;
            QVector<unsigned int> lodIndices;
            QVector<Meshlet> meshlets;
            QVector<MeshletLODLevel> levels;
            bool isValid() const { return !renderData.positions.isEmpty() && !lodIndices.isEmpty() && !meshlets.isEmpty(); }
        };

        MeshletLODData generateMeshletLODData() const;
        MeshletLODData generateMeshletLODData(const MeshletLODConfig& config) const;
        static int chooseMeshletLODLevel(const MeshletLODData& data, float projectedRadiusPixels);

        // 5. ボーン/モーフ (Deformer)
        // デフォーマは動的アトリビュート "boneWeights",
        // "boneWeightsExtra", "skinDQWeight", "skinMethod",
        // "blendShape_X" などとして表現可能
        void applySkinning(const QVector<QMatrix4x4>& boneMatrices);
        void applyDeformers(const QVector<QMatrix4x4>& boneMatrices);

        // バウンディング
        void updateBounds();
        QVector3D boundingBoxMin() const;
        QVector3D boundingBoxMax() const;
        // Bounding sphere derived from the AABB: center is the box midpoint,
        // radius is the max distance from the center. Cheap and adequate for
        // culling/LOD decisions; not a minimal Ritter sphere.
        QVector3D boundingSphereCenter() const;
        float boundingSphereRadius() const;

        // Area-weighted smooth vertex normals from polygon topology.
        // Creates the "normal" attribute when missing. Degenerate faces are
        // skipped; vertices left without area fall back to (0,0,1).
        void computeVertexNormals();

        // ファイルI/O (Assimp等との連携)
        bool loadFromFile(const QString& filePath);
        bool saveToFile(const QString& filePath) const;

        void clear();
        bool isValid() const;
    };

}
