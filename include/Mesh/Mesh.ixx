module;

// 全てのインクルードをグローバルモジュールフラグメント内に
#include <memory>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QtCore/QObject>
#include "../Define/DllExportMacro.hpp"

export module Mesh;

import std;

export namespace ArtifactCore {

// 頂点属性
struct Vertex {
    QVector3D position;
    QVector3D normal;
    QVector2D uv;
    QVector3D tangent;
    QVector3D bitangent;
    QVector4D color;
    QVector4D boneWeights;  // スキニング用
    QVector<int> boneIndices;  // 最大4本のボーン
};

// サブメッシュ（マテリアルグループ）
struct SubMesh {
    QString name;
    int materialIndex;
    int startIndex;
    int indexCount;
};

// ボーン情報
struct Bone {
    QString name;
    QMatrix4x4 offsetMatrix;
    int parentIndex;
};

// モーフターゲット/ブレンドシェイプ
struct MorphTarget {
    QString name;
    QVector<QVector3D> deltaPositions;
    QVector<QVector3D> deltaNormals;
};

// バウンディング情報
struct BoundingBox {
    QVector3D min;
    QVector3D max;
};

struct BoundingSphere {
    QVector3D center;
    float radius;
};

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

  // 基本データアクセス
  void setVertices(const QVector<Vertex>& vertices);
  QVector<Vertex> vertices() const;
  void setIndices(const QVector<unsigned int>& indices);
  QVector<unsigned int> indices() const;

  // サブメッシュ管理
  void addSubMesh(const SubMesh& subMesh);
  void removeSubMesh(int index);
  QVector<SubMesh> subMeshes() const;
  int subMeshCount() const;

  // ボーン/スキニング
  void setBones(const QVector<Bone>& bones);
  QVector<Bone> bones() const;
  void setSkinning(bool enabled);
  bool hasSkinning() const;

  // モーフターゲット
  void addMorphTarget(const MorphTarget& target);
  void removeMorphTarget(int index);
  QVector<MorphTarget> morphTargets() const;
  void setMorphWeight(int index, float weight);
  float morphWeight(int index) const;

  // バウンディング情報
  BoundingBox boundingBox() const;
  BoundingSphere boundingSphere() const;
  void updateBounds();

  // トポロジー情報
  int vertexCount() const;
  int triangleCount() const;
  int edgeCount() const;

  // メッシュ操作
  void recalculateNormals(bool smooth = true);
  void recalculateTangents();
  void optimize();  // 頂点キャッシュ最適化
  void weld(float threshold = 0.001f);  // 重複頂点の結合
  void subdivide(int level = 1);  // 細分化
  void simplify(float targetReduction);  // ポリゴン削減

  // UV操作
  void generateUVs();  // 自動UV展開
  void normalizeUVs();  // UV正規化

  // メッシュ変換
  void transform(const QMatrix4x4& matrix);
  void translate(const QVector3D& offset);
  void rotate(const QVector3D& axis, float angle);
  void scale(const QVector3D& scale);

  // メッシュ生成ヘルパー
  static Mesh createPlane(float width, float height, int segmentsX = 1, int segmentsY = 1);
  static Mesh createCube(float size = 1.0f);
  static Mesh createSphere(float radius = 1.0f, int segments = 32);
  static Mesh createCylinder(float radius = 1.0f, float height = 2.0f, int segments = 32);
  static Mesh createCone(float radius = 1.0f, float height = 2.0f, int segments = 32);
  static Mesh createTorus(float majorRadius = 1.0f, float minorRadius = 0.25f, int majorSegments = 32, int minorSegments = 16);

  // ファイルI/O
  bool loadFromFile(const QString& filePath);
  bool saveToFile(const QString& filePath) const;

  // メッシュ情報
  bool isEmpty() const;
  bool isValid() const;
  void clear();

  // デバッグ/検証
  bool validate() const;  // メッシュの整合性チェック
  QString statistics() const;  // 統計情報
 };

}