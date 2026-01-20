
module;

#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QFile>
#include <QDataStream>

module Mesh;

import std;

namespace ArtifactCore {

 class Mesh::Impl {
 public:
  QVector<Vertex> vertices_;
  QVector<unsigned int> indices_;
  QVector<SubMesh> subMeshes_;
  QVector<Bone> bones_;
  QVector<MorphTarget> morphTargets_;
  QVector<float> morphWeights_;
  
  BoundingBox boundingBox_;
  BoundingSphere boundingSphere_;
  
  bool hasSkinning_ = false;
  bool boundsDirty_ = true;

  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;

  void updateBounds() {
   if (vertices_.isEmpty()) {
    boundingBox_.min = QVector3D(0, 0, 0);
    boundingBox_.max = QVector3D(0, 0, 0);
    boundingSphere_.center = QVector3D(0, 0, 0);
    boundingSphere_.radius = 0;
    return;
   }

   // バウンディングボックス計算
   boundingBox_.min = vertices_[0].position;
   boundingBox_.max = vertices_[0].position;

   for (const auto& vertex : vertices_) {
    boundingBox_.min.setX(std::min(boundingBox_.min.x(), vertex.position.x()));
    boundingBox_.min.setY(std::min(boundingBox_.min.y(), vertex.position.y()));
    boundingBox_.min.setZ(std::min(boundingBox_.min.z(), vertex.position.z()));
    
    boundingBox_.max.setX(std::max(boundingBox_.max.x(), vertex.position.x()));
    boundingBox_.max.setY(std::max(boundingBox_.max.y(), vertex.position.y()));
    boundingBox_.max.setZ(std::max(boundingBox_.max.z(), vertex.position.z()));
   }

   // バウンディングスフィア計算
   boundingSphere_.center = (boundingBox_.min + boundingBox_.max) * 0.5f;
   boundingSphere_.radius = 0;

   for (const auto& vertex : vertices_) {
    float distance = (vertex.position - boundingSphere_.center).length();
    boundingSphere_.radius = std::max(boundingSphere_.radius, distance);
   }

   boundsDirty_ = false;
  }

  void recalculateNormals(bool smooth) {
   // 法線をリセット
   for (auto& vertex : vertices_) {
    vertex.normal = QVector3D(0, 0, 0);
   }

   // 各三角形の法線を計算して頂点に加算
   for (int i = 0; i < indices_.size(); i += 3) {
    unsigned int i0 = indices_[i];
    unsigned int i1 = indices_[i + 1];
    unsigned int i2 = indices_[i + 2];

    QVector3D v0 = vertices_[i0].position;
    QVector3D v1 = vertices_[i1].position;
    QVector3D v2 = vertices_[i2].position;

    QVector3D edge1 = v1 - v0;
    QVector3D edge2 = v2 - v0;
    QVector3D normal = QVector3D::crossProduct(edge1, edge2);

    if (smooth) {
     // スムーズシェーディング：法線を加算
     vertices_[i0].normal += normal;
     vertices_[i1].normal += normal;
     vertices_[i2].normal += normal;
    } else {
     // フラットシェーディング：同じ法線を設定
     normal.normalize();
     vertices_[i0].normal = normal;
     vertices_[i1].normal = normal;
     vertices_[i2].normal = normal;
    }
   }

   // スムーズシェーディングの場合は正規化
   if (smooth) {
    for (auto& vertex : vertices_) {
     vertex.normal.normalize();
    }
   }
  }
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

 void Mesh::setVertices(const QVector<Vertex>& vertices) {
  impl_->vertices_ = vertices;
  impl_->boundsDirty_ = true;
 }

 QVector<Vertex> Mesh::vertices() const {
  return impl_->vertices_;
 }

 void Mesh::setIndices(const QVector<unsigned int>& indices) {
  impl_->indices_ = indices;
 }

 QVector<unsigned int> Mesh::indices() const {
  return impl_->indices_;
 }

 void Mesh::addSubMesh(const SubMesh& subMesh) {
  impl_->subMeshes_.append(subMesh);
 }

 void Mesh::removeSubMesh(int index) {
  if (index >= 0 && index < impl_->subMeshes_.size()) {
   impl_->subMeshes_.remove(index);
  }
 }

 QVector<SubMesh> Mesh::subMeshes() const {
  return impl_->subMeshes_;
 }

 int Mesh::subMeshCount() const {
  return impl_->subMeshes_.size();
 }

 void Mesh::setBones(const QVector<Bone>& bones) {
  impl_->bones_ = bones;
 }

 QVector<Bone> Mesh::bones() const {
  return impl_->bones_;
 }

 void Mesh::setSkinning(bool enabled) {
  impl_->hasSkinning_ = enabled;
 }

 bool Mesh::hasSkinning() const {
  return impl_->hasSkinning_;
 }

 void Mesh::addMorphTarget(const MorphTarget& target) {
  impl_->morphTargets_.append(target);
  impl_->morphWeights_.append(0.0f);
 }

 void Mesh::removeMorphTarget(int index) {
  if (index >= 0 && index < impl_->morphTargets_.size()) {
   impl_->morphTargets_.remove(index);
   impl_->morphWeights_.remove(index);
  }
 }

 QVector<MorphTarget> Mesh::morphTargets() const {
  return impl_->morphTargets_;
 }

 void Mesh::setMorphWeight(int index, float weight) {
  if (index >= 0 && index < impl_->morphWeights_.size()) {
   impl_->morphWeights_[index] = weight;
  }
 }

 float Mesh::morphWeight(int index) const {
  if (index >= 0 && index < impl_->morphWeights_.size()) {
   return impl_->morphWeights_[index];
  }
  return 0.0f;
 }

 BoundingBox Mesh::boundingBox() const {
  if (impl_->boundsDirty_) {
   const_cast<Impl*>(impl_)->updateBounds();
  }
  return impl_->boundingBox_;
 }

 BoundingSphere Mesh::boundingSphere() const {
  if (impl_->boundsDirty_) {
   const_cast<Impl*>(impl_)->updateBounds();
  }
  return impl_->boundingSphere_;
 }

 void Mesh::updateBounds() {
  impl_->updateBounds();
 }

 int Mesh::vertexCount() const {
  return impl_->vertices_.size();
 }

 int Mesh::triangleCount() const {
  return impl_->indices_.size() / 3;
 }

 int Mesh::edgeCount() const {
  // 簡易計算：三角形数 * 3 / 2（共有エッジを考慮）
  return (impl_->indices_.size() / 3) * 3 / 2;
 }

 void Mesh::recalculateNormals(bool smooth) {
  impl_->recalculateNormals(smooth);
 }

 void Mesh::recalculateTangents() {
  // タンジェント計算（簡易実装）
  for (int i = 0; i < impl_->indices_.size(); i += 3) {
   unsigned int i0 = impl_->indices_[i];
   unsigned int i1 = impl_->indices_[i + 1];
   unsigned int i2 = impl_->indices_[i + 2];

   Vertex& v0 = impl_->vertices_[i0];
   Vertex& v1 = impl_->vertices_[i1];
   Vertex& v2 = impl_->vertices_[i2];

   QVector3D edge1 = v1.position - v0.position;
   QVector3D edge2 = v2.position - v0.position;

   QVector2D deltaUV1 = v1.uv - v0.uv;
   QVector2D deltaUV2 = v2.uv - v0.uv;

   float f = 1.0f / (deltaUV1.x() * deltaUV2.y() - deltaUV2.x() * deltaUV1.y());

   QVector3D tangent;
   tangent.setX(f * (deltaUV2.y() * edge1.x() - deltaUV1.y() * edge2.x()));
   tangent.setY(f * (deltaUV2.y() * edge1.y() - deltaUV1.y() * edge2.y()));
   tangent.setZ(f * (deltaUV2.y() * edge1.z() - deltaUV1.y() * edge2.z()));
   tangent.normalize();

   v0.tangent = tangent;
   v1.tangent = tangent;
   v2.tangent = tangent;

   // バイタンジェント計算
   v0.bitangent = QVector3D::crossProduct(v0.normal, v0.tangent);
   v1.bitangent = QVector3D::crossProduct(v1.normal, v1.tangent);
   v2.bitangent = QVector3D::crossProduct(v2.normal, v2.tangent);
  }
 }

 void Mesh::optimize() {
  // 頂点キャッシュ最適化（簡易実装）
  // TODO: より高度な最適化アルゴリズムの実装
 }

 void Mesh::weld(float threshold) {
  // 重複頂点の結合（簡易実装）
  QVector<Vertex> uniqueVertices;
  QVector<int> remapping(impl_->vertices_.size());

  for (int i = 0; i < impl_->vertices_.size(); ++i) {
   bool found = false;
   for (int j = 0; j < uniqueVertices.size(); ++j) {
    if ((impl_->vertices_[i].position - uniqueVertices[j].position).length() < threshold) {
     remapping[i] = j;
     found = true;
     break;
    }
   }
   if (!found) {
    remapping[i] = uniqueVertices.size();
    uniqueVertices.append(impl_->vertices_[i]);
   }
  }

  // インデックス更新
  for (auto& index : impl_->indices_) {
   index = remapping[index];
  }

  impl_->vertices_ = uniqueVertices;
  impl_->boundsDirty_ = true;
 }

 void Mesh::subdivide(int level) {
  // メッシュ細分化（簡易実装）
  // TODO: Catmull-Clark細分化などの実装
 }

 void Mesh::simplify(float targetReduction) {
  // ポリゴン削減（簡易実装）
  // TODO: エッジコラプス法などの実装
 }

 void Mesh::generateUVs() {
  // UV自動展開（簡易実装）
  // TODO: より高度なUV展開アルゴリズムの実装
 }

 void Mesh::normalizeUVs() {
  for (auto& vertex : impl_->vertices_) {
   vertex.uv.setX(vertex.uv.x() - std::floor(vertex.uv.x()));
   vertex.uv.setY(vertex.uv.y() - std::floor(vertex.uv.y()));
  }
 }

 void Mesh::transform(const QMatrix4x4& matrix) {
  for (auto& vertex : impl_->vertices_) {
   vertex.position = matrix.map(vertex.position);
   vertex.normal = matrix.mapVector(vertex.normal).normalized();
  }
  impl_->boundsDirty_ = true;
 }

 void Mesh::translate(const QVector3D& offset) {
  for (auto& vertex : impl_->vertices_) {
   vertex.position += offset;
  }
  impl_->boundsDirty_ = true;
 }

 void Mesh::rotate(const QVector3D& axis, float angle) {
  QMatrix4x4 rotation;
  rotation.rotate(angle, axis);
  transform(rotation);
 }

 void Mesh::scale(const QVector3D& scale) {
  for (auto& vertex : impl_->vertices_) {
   vertex.position.setX(vertex.position.x() * scale.x());
   vertex.position.setY(vertex.position.y() * scale.y());
   vertex.position.setZ(vertex.position.z() * scale.z());
  }
  impl_->boundsDirty_ = true;
 }

 Mesh Mesh::createPlane(float width, float height, int segmentsX, int segmentsY) {
  Mesh mesh;
  QVector<Vertex> vertices;
  QVector<unsigned int> indices;

  // 頂点生成
  for (int y = 0; y <= segmentsY; ++y) {
   for (int x = 0; x <= segmentsX; ++x) {
    Vertex v;
    v.position = QVector3D(
     (x / float(segmentsX) - 0.5f) * width,
     0,
     (y / float(segmentsY) - 0.5f) * height
    );
    v.normal = QVector3D(0, 1, 0);
    v.uv = QVector2D(x / float(segmentsX), y / float(segmentsY));
    v.color = QVector4D(1, 1, 1, 1);
    vertices.append(v);
   }
  }

  // インデックス生成
  for (int y = 0; y < segmentsY; ++y) {
   for (int x = 0; x < segmentsX; ++x) {
    int i0 = y * (segmentsX + 1) + x;
    int i1 = i0 + 1;
    int i2 = i0 + (segmentsX + 1);
    int i3 = i2 + 1;

    indices.append(i0);
    indices.append(i2);
    indices.append(i1);

    indices.append(i1);
    indices.append(i2);
    indices.append(i3);
   }
  }

  mesh.setVertices(vertices);
  mesh.setIndices(indices);
  return mesh;
 }

 Mesh Mesh::createCube(float size) {
  Mesh mesh;
  float half = size * 0.5f;
  
  // 簡易実装：24頂点（各面4頂点）
  QVector<Vertex> vertices;
  QVector<unsigned int> indices;

  // TODO: 完全な立方体メッシュの実装

  mesh.setVertices(vertices);
  mesh.setIndices(indices);
  return mesh;
 }

 Mesh Mesh::createSphere(float radius, int segments) {
  Mesh mesh;
  // TODO: 球体メッシュの実装
  return mesh;
 }

 Mesh Mesh::createCylinder(float radius, float height, int segments) {
  Mesh mesh;
  // TODO: 円柱メッシュの実装
  return mesh;
 }

 Mesh Mesh::createCone(float radius, float height, int segments) {
  Mesh mesh;
  // TODO: 円錐メッシュの実装
  return mesh;
 }

 Mesh Mesh::createTorus(float majorRadius, float minorRadius, int majorSegments, int minorSegments) {
  Mesh mesh;
  // TODO: トーラスメッシュの実装
  return mesh;
 }

 bool Mesh::loadFromFile(const QString& filePath) {
  // TODO: ファイルローダーの実装（FBX, OBJ, GLTFなど）
  return false;
 }

 bool Mesh::saveToFile(const QString& filePath) const {
  // TODO: ファイルセーバーの実装
  return false;
 }

 bool Mesh::isEmpty() const {
  return impl_->vertices_.isEmpty();
 }

 bool Mesh::isValid() const {
  return !impl_->vertices_.isEmpty() && !impl_->indices_.isEmpty();
 }

 void Mesh::clear() {
  impl_->vertices_.clear();
  impl_->indices_.clear();
  impl_->subMeshes_.clear();
  impl_->bones_.clear();
  impl_->morphTargets_.clear();
  impl_->morphWeights_.clear();
  impl_->boundsDirty_ = true;
 }

 bool Mesh::validate() const {
  // メッシュの整合性チェック
  if (impl_->vertices_.isEmpty()) return false;
  if (impl_->indices_.size() % 3 != 0) return false;
  
  for (auto index : impl_->indices_) {
   if (index >= static_cast<unsigned int>(impl_->vertices_.size())) {
    return false;
   }
  }
  
  return true;
 }

 QString Mesh::statistics() const {
  QString stats;
  stats += QString("Vertices: %1\n").arg(vertexCount());
  stats += QString("Triangles: %1\n").arg(triangleCount());
  stats += QString("Edges: %1\n").arg(edgeCount());
  stats += QString("SubMeshes: %1\n").arg(subMeshCount());
  stats += QString("Bones: %1\n").arg(impl_->bones_.size());
  stats += QString("Morph Targets: %1\n").arg(impl_->morphTargets_.size());
  return stats;
 }

}
