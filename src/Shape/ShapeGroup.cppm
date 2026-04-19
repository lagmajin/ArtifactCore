module;

#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>
#include <QTransform>

module Shape.Group:Impl;

import Shape.Group;

namespace ArtifactCore {

class PathShape::Impl {
public:
    ShapePath path_;
    StrokeSettings stroke_;
    FillSettings fill_;
};

// ========================================
// ShapeElement 実装
// ========================================

// ShapeElement は抽象基底クラスのため、関数実装なし

// ========================================
// ShapeGroup 実装
// ========================================

ShapeGroup::ShapeGroup() = default;

ShapeGroup::~ShapeGroup() = default;

void ShapeGroup::addChild(std::unique_ptr<ShapeElement> child) {
    if (child) {
        child->parent_ = this;
        children_.push_back(std::move(child));
    }
}

void ShapeGroup::insertChild(int index, std::unique_ptr<ShapeElement> child) {
    if (!child) return;
    child->parent_ = this;
    if (index < 0 || index >= static_cast<int>(children_.size())) {
        children_.push_back(std::move(child));
    } else {
        children_.insert(children_.begin() + index, std::move(child));
    }
}

void ShapeGroup::removeChild(ShapeElement* child) {
    if (!child) return;
    auto it = std::find_if(children_.begin(), children_.end(),
        [child](const std::unique_ptr<ShapeElement>& ptr) { return ptr.get() == child; });
    if (it != children_.end()) {
        (*it)->parent_ = nullptr;
        children_.erase(it);
    }
}

void ShapeGroup::removeChild(int index) {
    if (index >= 0 && index < static_cast<int>(children_.size())) {
        children_[index]->parent_ = nullptr;
        children_.erase(children_.begin() + index);
    }
}

void ShapeGroup::clearChildren() {
    for (auto& child : children_) {
        child->parent_ = nullptr;
    }
    children_.clear();
}

int ShapeGroup::childCount() const {
    return static_cast<int>(children_.size());
}

ShapeElement* ShapeGroup::childAt(int index) const {
    if (index < 0 || index >= static_cast<int>(children_.size())) return nullptr;
    return children_[index].get();
}

std::vector<ShapeElement*> ShapeGroup::children() const {
    std::vector<ShapeElement*> result;
    result.reserve(children_.size());
    for (const auto& child : children_) {
        result.push_back(child.get());
    }
    return result;
}

int ShapeGroup::indexOf(ShapeElement* child) const {
    if (!child) return -1;
    for (int i = 0; i < static_cast<int>(children_.size()); ++i) {
        if (children_[i].get() == child) return i;
    }
    return -1;
}

QRectF ShapeGroup::boundingRect() const {
    if (children_.empty()) return QRectF();

    QRectF bounds;
    bool first = true;
    for (const auto& child : children_) {
        QRectF childBounds = child->boundingRect();
        if (!childBounds.isNull()) {
            if (first) {
                bounds = childBounds;
                first = false;
            } else {
                bounds = bounds.united(childBounds);
            }
        }
    }
    const auto& self = transform();
    QTransform matrix;
    matrix.translate(self.position.x, self.position.y);
    matrix.translate(self.anchor.x, self.anchor.y);
    matrix.rotate(self.rotation);
    matrix.scale(self.scale.x, self.scale.y);
    matrix.translate(-self.anchor.x, -self.anchor.y);
    return matrix.mapRect(bounds);
}

void ShapeGroup::translate(const QPointF& offset) {
    for (auto& child : children_) {
        auto& childTransform = child->transform();
        childTransform.position = childTransform.position + Point2DValue(offset);
    }
    transform().position = transform().position + Point2DValue(offset);
}

void ShapeGroup::scale(const QPointF& center, double sx, double sy) {
    for (auto& child : children_) {
        auto& childTransform = child->transform();
        childTransform.position = Point2DValue(
            center.x() + (childTransform.position.x - center.x()) * sx,
            center.y() + (childTransform.position.y - center.y()) * sy);
        childTransform.scale = Point2DValue(childTransform.scale.x * sx, childTransform.scale.y * sy);
    }
    auto& selfTransform = transform();
    selfTransform.position = Point2DValue(
        center.x() + (selfTransform.position.x - center.x()) * sx,
        center.y() + (selfTransform.position.y - center.y()) * sy);
    selfTransform.scale = Point2DValue(selfTransform.scale.x * sx, selfTransform.scale.y * sy);
}

void ShapeGroup::rotate(const QPointF& center, double angle) {
    const double radians = angle * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(radians);
    const double sinA = std::sin(radians);
    for (auto& child : children_) {
        auto& childTransform = child->transform();
        const double dx = childTransform.position.x - center.x();
        const double dy = childTransform.position.y - center.y();
        childTransform.position = Point2DValue(
            center.x() + dx * cosA - dy * sinA,
            center.y() + dx * sinA + dy * cosA);
        childTransform.rotation += angle;
    }
    auto& selfTransform = transform();
    const double dx = selfTransform.position.x - center.x();
    const double dy = selfTransform.position.y - center.y();
    selfTransform.position = Point2DValue(
        center.x() + dx * cosA - dy * sinA,
        center.y() + dx * sinA + dy * cosA);
    selfTransform.rotation += angle;
}

std::unique_ptr<ShapeElement> ShapeGroup::clone() const {
    auto copy = std::make_unique<ShapeGroup>();
    copy->setName(name_);
    copy->setVisible(visible_);
    copy->setLocked(locked_);
    copy->transform_ = transform_;
    for (const auto& child : children_) {
        copy->addChild(child->clone());
    }
    // 演算子もクローン
    for (const auto& op : operators_) {
        // ShapeOperator のクローン実装が必要（未実装の場合はskip）
        // TODO: ShapeOperator に clone() を実装
    }
    return copy;
}

void ShapeGroup::addOperator(std::unique_ptr<ShapeOperator> op) {
    if (op) {
        operators_.push_back(std::move(op));
    }
}

ShapeOperator* ShapeGroup::operatorAt(int index) const {
    if (index < 0 || index >= static_cast<int>(operators_.size())) {
        return nullptr;
    }
    return operators_[index].get();
}

std::vector<ShapePath> ShapeGroup::processedPaths() const {
    std::vector<ShapePath> paths;
    auto makeMatrix = [](const ShapeTransform& tf) {
        QTransform matrix;
        matrix.translate(tf.position.x, tf.position.y);
        matrix.translate(tf.anchor.x, tf.anchor.y);
        matrix.rotate(tf.rotation);
        matrix.scale(tf.scale.x, tf.scale.y);
        matrix.translate(-tf.anchor.x, -tf.anchor.y);
        return matrix;
    };
    std::function<void(const ShapeGroup&, const QTransform&)> collectPaths = [&](const ShapeGroup& group, const QTransform& parentMatrix) {
        const QTransform groupMatrix = parentMatrix * makeMatrix(group.transform());
        for (const auto& child : group.children_) {
            if (auto* pathShape = dynamic_cast<PathShape*>(child.get())) {
                ShapePath path = ShapePath::fromPainterPath(pathShape->toPainterPath());
                path.transform(groupMatrix);
                paths.push_back(path);
            } else if (auto* childGroup = dynamic_cast<ShapeGroup*>(child.get())) {
                collectPaths(*childGroup, groupMatrix);
            }
        }
    };
    collectPaths(*this, QTransform());
    for (const auto& op : operators_) {
        paths = op->process(paths);
    }
    return paths;
}

// ========================================
// PathShape 実装
// ========================================

PathShape::PathShape() : impl_(new Impl()) {}

PathShape::~PathShape() { delete impl_; }

ShapePath& PathShape::path() {
    return impl_->path_;
}

const ShapePath& PathShape::path() const {
    return impl_->path_;
}

void PathShape::setPath(const ShapePath& path) {
    impl_->path_ = path;
}

StrokeSettings& PathShape::stroke() {
    return impl_->stroke_;
}

const StrokeSettings& PathShape::stroke() const {
    return impl_->stroke_;
}

void PathShape::setStroke(const StrokeSettings& stroke) {
    impl_->stroke_ = stroke;
}

FillSettings& PathShape::fill() {
    return impl_->fill_;
}

const FillSettings& PathShape::fill() const {
    return impl_->fill_;
}

void PathShape::setFill(const FillSettings& fill) {
    impl_->fill_ = fill;
}

QRectF PathShape::boundingRect() const {
    return toPainterPath().boundingRect();
}

QPainterPath PathShape::toPainterPath() const {
    QPainterPath qpath = path().toPainterPath();
    const auto& tf = transform();
    QTransform matrix;
    matrix.translate(tf.position.x, tf.position.y);
    matrix.translate(tf.anchor.x, tf.anchor.y);
    matrix.rotate(tf.rotation);
    matrix.scale(tf.scale.x, tf.scale.y);
    matrix.translate(-tf.anchor.x, -tf.anchor.y);
    qpath = matrix.map(qpath);

    // ストローク設定が有効なら QPainterPathStroker で変換
    // TODO: 実装

    // フィル設定は描画時に使用するため、ここではパスのみ返す
    return qpath;
}

std::unique_ptr<ShapeElement> PathShape::clone() const {
    auto copy = std::make_unique<PathShape>();
    copy->setName(name_);
    copy->setVisible(visible_);
    copy->setLocked(locked_);
    copy->transform() = transform();
    copy->setPath(path());
    copy->setStroke(stroke());
    copy->setFill(fill());
    return copy;
}

// ========================================
// RectanglePathShape 実装
// ========================================

RectanglePathShape::RectanglePathShape() = default;

RectanglePathShape::RectanglePathShape(const QRectF& rect) : rect_(rect) {
    updatePath();
}

QRectF RectanglePathShape::rect() const { return rect_; }

void RectanglePathShape::setRect(const QRectF& rect) {
    if (rect_ != rect) {
        rect_ = rect;
        updatePath();
    }
}

double RectanglePathShape::cornerRadius() const { return cornerRadius_; }

void RectanglePathShape::setCornerRadius(double radius) {
    if (cornerRadius_ != radius) {
        cornerRadius_ = radius;
        updatePath();
    }
}

std::unique_ptr<ShapeElement> RectanglePathShape::clone() const {
    auto copy = std::make_unique<RectanglePathShape>(rect_);
    copy->setName(name_);
    copy->setVisible(visible_);
    copy->setLocked(locked_);
    copy->transform() = transform();
    copy->setCornerRadius(cornerRadius_);
    copy->setStroke(stroke());
    copy->setFill(fill());
    return copy;
}

void RectanglePathShape::updatePath() {
    if (cornerRadius_ > 0.0) {
        // 角丸矩形（円弧）
        PathShape::setPath(ShapePath()); // 一旦クリア
        // TODO: 角丸矩形のパス構築
        // 暫定：角丸なし矩形として設定
        ShapePath path;
        path.setRectangle(rect_);
        PathShape::setPath(path);
    } else {
        ShapePath path;
        path.setRectangle(rect_);
        PathShape::setPath(path);
    }
}

// ========================================
// EllipsePathShape 実装
// ========================================

EllipsePathShape::EllipsePathShape() = default;

EllipsePathShape::EllipsePathShape(const QRectF& rect) : rect_(rect) {
    updatePath();
}

QRectF EllipsePathShape::rect() const { return rect_; }

void EllipsePathShape::setRect(const QRectF& rect) {
    if (rect_ != rect) {
        rect_ = rect;
        updatePath();
    }
}

std::unique_ptr<ShapeElement> EllipsePathShape::clone() const {
    auto copy = std::make_unique<EllipsePathShape>(rect_);
    copy->setName(name_);
    copy->setVisible(visible_);
    copy->setLocked(locked_);
    copy->transform() = transform();
    copy->setStroke(stroke());
    copy->setFill(fill());
    return copy;
}

void EllipsePathShape::updatePath() {
    ShapePath path;
    path.setEllipse(rect_);
    PathShape::setPath(path);
}

// ========================================
// PolygonPathShape 実装
// ========================================

PolygonPathShape::PolygonPathShape() = default;

PolygonPathShape::PolygonPathShape(const std::vector<QPointF>& points) : points_(points) {
    updatePath();
}

std::vector<QPointF> PolygonPathShape::points() const { return points_; }

void PolygonPathShape::setPoints(const std::vector<QPointF>& points) {
    points_ = points;
    updatePath();
}

int PolygonPathShape::pointCount() const { return static_cast<int>(points_.size()); }

bool PolygonPathShape::isClosed() const { return closed_; }

void PolygonPathShape::setClosed(bool closed) {
    if (closed_ != closed) {
        closed_ = closed;
        updatePath();
    }
}

std::unique_ptr<ShapeElement> PolygonPathShape::clone() const {
    auto copy = std::make_unique<PolygonPathShape>(points_);
    copy->setName(name_);
    copy->setVisible(visible_);
    copy->setLocked(locked_);
    copy->transform() = transform();
    copy->setClosed(closed_);
    copy->setStroke(stroke());
    copy->setFill(fill());
    return copy;
}

void PolygonPathShape::updatePath() {
    ShapePath path;
    path.setPolygon(points_, closed_);
    PathShape::setPath(path);
}

// ========================================
// StarPathShape 実装
// ========================================

StarPathShape::StarPathShape() = default;

StarPathShape::StarPathShape(const QPointF& center, int points, double outerRadius, double innerRadius)
    : center_(center), points_(points), outerRadius_(outerRadius), innerRadius_(innerRadius) {
    updatePath();
}

QPointF StarPathShape::center() const { return center_; }

void StarPathShape::setCenter(const QPointF& center) {
    if (center_ != center) {
        center_ = center;
        updatePath();
    }
}

int StarPathShape::points() const { return points_; }

void StarPathShape::setPoints(int points) {
    if (points_ != points) {
        points_ = points;
        updatePath();
    }
}

double StarPathShape::outerRadius() const { return outerRadius_; }

void StarPathShape::setOuterRadius(double radius) {
    if (outerRadius_ != radius) {
        outerRadius_ = radius;
        updatePath();
    }
}

double StarPathShape::innerRadius() const { return innerRadius_; }

void StarPathShape::setInnerRadius(double radius) {
    if (innerRadius_ != radius) {
        innerRadius_ = radius;
        updatePath();
    }
}

std::unique_ptr<ShapeElement> StarPathShape::clone() const {
    auto copy = std::make_unique<StarPathShape>(center_, points_, outerRadius_, innerRadius_);
    copy->setName(name_);
    copy->setVisible(visible_);
    copy->setLocked(locked_);
    copy->transform() = transform();
    copy->setStroke(stroke());
    copy->setFill(fill());
    return copy;
}

void StarPathShape::updatePath() {
    ShapePath path;
    path.setStar(center_, points_, outerRadius_, innerRadius_);
    PathShape::setPath(path);
}

} // namespace ArtifactCore
