module;
#include <utility>
#include <memory>
#include <QString>
#include <QVector>
#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>
#include <vector>

export module Scene.SceneNode;

import Mesh;
import Material.Material;
import Utils.String.UniString;

export namespace ArtifactCore
{

 /// 3D シーンノード。DCC 標準のヒエラルキー (Maya Outliner / 3ds Max Scene Explorer 相当)。
 /// 親子関係、ローカル/ワールド変換、メッシュ/マテリアル割当を持つ。
 class SceneNode {
 public:
  SceneNode();
  explicit SceneNode(const UniString& name);
  SceneNode(const SceneNode&) = delete;
  SceneNode& operator=(const SceneNode&) = delete;
  ~SceneNode();

  // --- Identity ---
  void setName(const UniString& name);
  UniString name() const;

  // --- Hierarchy ---

  void addChild(std::shared_ptr<SceneNode> child);
  void removeChild(const SceneNode* child);
  void removeFromParent();
  int childCount() const;
  SceneNode* child(int index) const;
  SceneNode* parent() const;
  std::vector<SceneNode*> children() const;
  std::vector<SceneNode*> descendants() const;

  /// ルートへのパス (例: "/root/character/head")
  QString path() const;

  /// 名前で子孫を検索 (最初に見つかったもの)
  SceneNode* findByName(const UniString& name) const;

  // --- Local Transform ---

  void setPosition(const QVector3D& pos);
  QVector3D position() const;
  void setRotation(const QQuaternion& rot);
  QQuaternion rotation() const;
  void setRotationEuler(const QVector3D& eulerDeg);
  QVector3D rotationEuler() const;
  void setScale(const QVector3D& scale);
  QVector3D scale() const;

  /// ローカル行列 (SRT)
  QMatrix4x4 localMatrix() const;

  // --- World Transform ---

  /// 親のワールド行列 × ローカル行列
  QMatrix4x4 worldMatrix() const;

  /// ワールド空間の位置
  QVector3D worldPosition() const;

  // --- Content ---

  void setMesh(std::shared_ptr<Mesh> mesh);
  std::shared_ptr<Mesh> mesh() const;
  void setMaterial(std::shared_ptr<Material> material);
  std::shared_ptr<Material> material() const;

  // --- Visibility ---

  void setVisible(bool visible);
  bool isVisible() const;

  // --- Utility ---

  /// 子孫を含むバウンディングボックス
  void boundingBox(QVector3D& outMin, QVector3D& outMax) const;

  /// ノード数 (自分 + 子孫)
  int totalNodeCount() const;

 private:
  class Impl;
  Impl* impl_;
 };

}
