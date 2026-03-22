module;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
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
export module Mesh;





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
        std::unordered_map<std::string, std::shared_ptr<MeshAttributeBase>> attributes_;
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
        std::shared_ptr<MeshAttribute<T>> add(const std::string& name) {
            auto attr = std::make_shared<MeshAttribute<T>>();
            attr->resize(elementCount_);
            attributes_[name] = attr;
            return attr;
        }

        template<typename T>
        std::shared_ptr<MeshAttribute<T>> get(const std::string& name) const {
            auto it = attributes_.find(name);
            if (it != attributes_.end() && it->second->type() == typeid(T)) {
                return std::static_pointer_cast<MeshAttribute<T>>(it->second);
            }
            return nullptr;
        }

        bool has(const std::string& name) const { return attributes_.count(name) > 0; }
        std::vector<std::string> attributeNames() const {
            std::vector<std::string> names;
            for (const auto& pair : attributes_) names.push_back(pair.first);
            return names;
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
        Mesh();
        Mesh(const Mesh& other);
        Mesh(Mesh&& other) noexcept;
        ~Mesh();

        Mesh& operator=(const Mesh& other);
        Mesh& operator=(Mesh&& other) noexcept;

        // 1. トポロジー構築 (N-gon対応)
        // Mayaのように、頂点のリストと、それらを結ぶ「面（ポリゴン）」のリストで構成
        void setVertexCount(int count);
        int vertexCount() const;

        // 面を追加 (3角形、4角形、N角形すべてを許容)
        // 戻り値は追加されたFaceのインデックス
        int addPolygon(const QVector<int>& vertexIndices);
        int polygonCount() const;

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

        // 3. 高度なトポロジー参照 (Half-Edge相当のクエリ)
        // ある頂点に接続しているすべての面を取得する
        QVector<int> getConnectedPolygons(int vertexIndex) const;
        // ある面に含まれる頂点インデックスを取得する
        QVector<int> getPolygonVertices(int polygonIndex) const;

        // 4. サブディビジョンと非破壊モディファイアの基盤
        // サブディビジョンサーフェス（Catmull-Clark等）を適用した新しいメッシュを生成
        std::shared_ptr<Mesh> createSubdivided(int level) const;
        
        // レンダリング用（GPU用）に、すべてを三角形に分割したフラットな配列を生成する
        struct RenderData {
            QVector<QVector3D> positions;
            QVector<QVector3D> normals;
            QVector<QVector2D> uvs;
            QVector<unsigned int> indices;
        };
        RenderData generateRenderData() const;

        // 5. ボーン/モーフ (Deformer)
        // デフォーマは動的アトリビュート "boneWeights", "blendShape_X" などとして表現可能
        void applySkinning(const QVector<QMatrix4x4>& boneMatrices);

        // バウンディング
        void updateBounds();
        QVector3D boundingBoxMin() const;
        QVector3D boundingBoxMax() const;

        // ファイルI/O (Assimp等との連携)
        bool loadFromFile(const QString& filePath);
        bool saveToFile(const QString& filePath) const;

        void clear();
        bool isValid() const;
    };

}