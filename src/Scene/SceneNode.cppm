module;
#include <utility>
#include <memory>
#include <QString>
#include <QVector>
#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>
#include <vector>
#include <algorithm>
#include <cmath>

module Scene.SceneNode;

import Mesh;
import Material.Material;
import Utils.String.UniString;

namespace ArtifactCore
{

class SceneNode::Impl {
public:
 UniString name_ = "Node";
 SceneNode* parent_ = nullptr;
 std::vector<std::shared_ptr<SceneNode>> children_;

 QVector3D position_ = { 0, 0, 0 };
 QQuaternion rotation_;
 QVector3D scale_ = { 1, 1, 1 };

 std::shared_ptr<Mesh> mesh_;
 std::shared_ptr<Material> material_;
 bool visible_ = true;

 mutable QMatrix4x4 cachedWorld_;
 mutable bool worldDirty_ = true;

 void markDirty()
 {
  worldDirty_ = true;
  for (auto& c : children_) {
   if (c && c->impl_) c->impl_->markDirty();
  }
 }

 QMatrix4x4 buildLocalMatrix() const
 {
  QMatrix4x4 m;
  m.translate(position_);
  m.rotate(rotation_);
  m.scale(scale_);
  return m;
 }
};

SceneNode::SceneNode() : impl_(new Impl()) {}
SceneNode::SceneNode(const UniString& name) : impl_(new Impl()) { impl_->name_ = name; }
SceneNode::~SceneNode() { delete impl_; }

// Identity
void SceneNode::setName(const UniString& name) { impl_->name_ = name; }
UniString SceneNode::name() const { return impl_->name_; }

// Hierarchy
void SceneNode::addChild(std::shared_ptr<SceneNode> child)
{
 if (!child) return;
 if (child->impl_->parent_) {
  child->impl_->parent_->removeChild(child.get());
 }
 child->impl_->parent_ = this;
 child->impl_->markDirty();
 impl_->children_.push_back(child);
}

void SceneNode::removeChild(const SceneNode* child)
{
 if (!child) return;
 auto& ch = impl_->children_;
 ch.erase(std::remove_if(ch.begin(), ch.end(),
  [child](const std::shared_ptr<SceneNode>& c) { return c.get() == child; }),
  ch.end());
}

void SceneNode::removeFromParent()
{
 if (impl_->parent_) {
  impl_->parent_->removeChild(this);
  impl_->parent_ = nullptr;
 }
}

int SceneNode::childCount() const { return static_cast<int>(impl_->children_.size()); }

SceneNode* SceneNode::child(int index) const
{
 if (index >= 0 && index < static_cast<int>(impl_->children_.size()))
  return impl_->children_[index].get();
 return nullptr;
}

SceneNode* SceneNode::parent() const { return impl_->parent_; }

std::vector<SceneNode*> SceneNode::children() const
{
 std::vector<SceneNode*> result;
 result.reserve(impl_->children_.size());
 for (auto& c : impl_->children_) result.push_back(c.get());
 return result;
}

std::vector<SceneNode*> SceneNode::descendants() const
{
 std::vector<SceneNode*> result;
 for (auto& c : impl_->children_) {
  result.push_back(c.get());
  auto sub = c->descendants();
  result.insert(result.end(), sub.begin(), sub.end());
 }
 return result;
}

QString SceneNode::path() const
 {
  QStringList parts;
  const SceneNode* n = this;
  while (n) {
   parts.prepend(n->impl_->name_.toQString());
   n = n->impl_->parent_;
  }
  return "/" + parts.join("/");
 }

SceneNode* SceneNode::findByName(const UniString& name) const
{
 if (impl_->name_ == name) return const_cast<SceneNode*>(this);
 for (auto& c : impl_->children_) {
  if (auto found = c->findByName(name)) return found;
 }
 return nullptr;
}

// Local Transform
void SceneNode::setPosition(const QVector3D& pos)
{
 impl_->position_ = pos;
 impl_->markDirty();
}

QVector3D SceneNode::position() const { return impl_->position_; }

void SceneNode::setRotation(const QQuaternion& rot)
{
 impl_->rotation_ = rot;
 impl_->markDirty();
}

QQuaternion SceneNode::rotation() const { return impl_->rotation_; }

void SceneNode::setRotationEuler(const QVector3D& eulerDeg)
{
 impl_->rotation_ = QQuaternion::fromEulerAngles(eulerDeg);
 impl_->markDirty();
}

QVector3D SceneNode::rotationEuler() const
{
 return impl_->rotation_.toEulerAngles();
}

void SceneNode::setScale(const QVector3D& s)
{
 impl_->scale_ = s;
 impl_->markDirty();
}

QVector3D SceneNode::scale() const { return impl_->scale_; }

QMatrix4x4 SceneNode::localMatrix() const
{
 return impl_->buildLocalMatrix();
}

// World Transform
QMatrix4x4 SceneNode::worldMatrix() const
{
 if (impl_->worldDirty_) {
  if (impl_->parent_) {
   impl_->cachedWorld_ = impl_->parent_->worldMatrix() * impl_->buildLocalMatrix();
  } else {
   impl_->cachedWorld_ = impl_->buildLocalMatrix();
  }
  impl_->worldDirty_ = false;
 }
 return impl_->cachedWorld_;
}

QVector3D SceneNode::worldPosition() const
{
 return worldMatrix().column(3).toVector3D();
}

// Content
void SceneNode::setMesh(std::shared_ptr<Mesh> mesh) { impl_->mesh_ = mesh; }
std::shared_ptr<Mesh> SceneNode::mesh() const { return impl_->mesh_; }
void SceneNode::setMaterial(std::shared_ptr<Material> material) { impl_->material_ = material; }
std::shared_ptr<Material> SceneNode::material() const { return impl_->material_; }

// Visibility
void SceneNode::setVisible(bool visible) { impl_->visible_ = visible; }
bool SceneNode::isVisible() const { return impl_->visible_; }

// Utility
void SceneNode::boundingBox(QVector3D& outMin, QVector3D& outMax) const
{
 bool first = true;
 std::function<void(const SceneNode*)> accumulate = [&](const SceneNode* node) {
  if (!node->isVisible()) return;
  if (node->impl_->mesh_ && node->impl_->mesh_->isValid()) {
   auto mn = node->impl_->mesh_->boundingBoxMin();
   auto mx = node->impl_->mesh_->boundingBoxMax();
   QMatrix4x4 wm = node->worldMatrix();
   // Transform 8 corners of the AABB
   for (int i = 0; i < 8; ++i) {
    QVector3D corner(
     (i & 1) ? mx.x() : mn.x(),
     (i & 2) ? mx.y() : mn.y(),
     (i & 4) ? mx.z() : mn.z()
    );
    QVector3D wc = wm.map(corner);
    if (first) { outMin = outMax = wc; first = false; }
    else {
     outMin.setX(std::min(outMin.x(), wc.x()));
     outMin.setY(std::min(outMin.y(), wc.y()));
     outMin.setZ(std::min(outMin.z(), wc.z()));
     outMax.setX(std::max(outMax.x(), wc.x()));
     outMax.setY(std::max(outMax.y(), wc.y()));
     outMax.setZ(std::max(outMax.z(), wc.z()));
    }
   }
  }
  for (auto& c : node->impl_->children_) accumulate(c.get());
 };
 accumulate(this);
 if (first) { outMin = outMax = QVector3D(0,0,0); }
}

int SceneNode::totalNodeCount() const
{
 int count = 1;
 for (auto& c : impl_->children_) count += c->totalNodeCount();
 return count;
}

}
