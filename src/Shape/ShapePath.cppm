module;

#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <QTransform>
#include <QJsonArray>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iterator>
#include <cfloat>
#include <limits>
#include <numbers>

module Shape.Path:Impl;

import Container.NamedVector;
import Shape.Path;
import Shape.Types;
import Math;
import Serialization.JsonAdapter;
import Serialization.SchemaMigration;

namespace ArtifactCore {

namespace {
const bool kShapePathSerializationRegistered = [] {
    Serialization::registerJsonSerializableType<ShapePath>(QStringLiteral("ShapePath"), 1);
    auto& migrations = Serialization::SchemaMigrationRegistry::instance();
    migrations.registerMigration(QStringLiteral("ShapePath"), 0, 1,
                                  [](const QJsonObject& object) { return object; });
    return true;
}();
}

// ========================================
// ShapePath::Impl 定義
// ========================================

class ShapePath::Impl {
public:
    QString name_;
    std::vector<PathCommand> commands_;
    PathFillRule fillRule_ = PathFillRule::Winding;
    double opacity_ = 1.0;
    mutable QRectF cachedBounds_;
    mutable bool dirty_ = true;

    Impl() = default;
    Impl(const Impl& other) = default;
    Impl& operator=(const Impl& other) = default;
    Impl(Impl&&) noexcept = default;

    void invalidate() const { dirty_ = true; }

    QRectF computeBounds() const {
        bool hasPoint = false;
        double minX = 0.0;
        double minY = 0.0;
        double maxX = 0.0;
        double maxY = 0.0;

        const auto include = [&](const QPointF& point) {
            const double x = point.x();
            const double y = point.y();
            if (!std::isfinite(x) || !std::isfinite(y)) return;
            if (!hasPoint) {
                minX = maxX = x;
                minY = maxY = y;
                hasPoint = true;
                return;
            }
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        };

        const auto quadPoint = [](const QPointF& p0, const QPointF& p1,
                                  const QPointF& p2, double t) {
            const double u = 1.0 - t;
            return u * u * p0 + 2.0 * u * t * p1 + t * t * p2;
        };
        const auto cubicPoint = [](const QPointF& p0, const QPointF& p1,
                                   const QPointF& p2, const QPointF& p3,
                                   double t) {
            const double u = 1.0 - t;
            return u * u * u * p0 + 3.0 * u * u * t * p1 +
                   3.0 * u * t * t * p2 + t * t * t * p3;
        };
        const auto includeRoot = [](double root, const auto& includeAt) {
            if (std::isfinite(root) && root > 0.0 && root < 1.0) includeAt(root);
        };

        const auto quadRoots = [&](double p0, double p1, double p2,
                                   const auto& includeAt) {
            const double denominator = p0 - 2.0 * p1 + p2;
            if (std::abs(denominator) <= 1e-12) return;
            includeRoot((p0 - p1) / denominator, includeAt);
        };

        const auto cubicRoots = [&](double p0, double p1, double p2, double p3,
                                    const auto& includeAt) {
            const double a = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
            const double b = 2.0 * (p0 - 2.0 * p1 + p2);
            const double c = p1 - p0;
            if (std::abs(a) <= 1e-12) {
                if (std::abs(b) > 1e-12) includeRoot(-c / b, includeAt);
                return;
            }
            const double discriminant = b * b - 4.0 * a * c;
            if (discriminant < 0.0) return;
            const double root = std::sqrt(std::max(0.0, discriminant));
            includeRoot((-b + root) / (2.0 * a), includeAt);
            includeRoot((-b - root) / (2.0 * a), includeAt);
        };

        QPointF current;
        bool hasCurrent = false;

        for (const auto& command : commands_) {
            switch (command.type) {
                case PathCommandType::MoveTo:
                    current = command.points[0];
                    hasCurrent = true;
                    include(current);
                    break;
                case PathCommandType::LineTo:
                    if (!hasCurrent) break;
                    include(current);
                    include(command.points[0]);
                    current = command.points[0];
                    break;
                case PathCommandType::QuadTo:
                    if (!hasCurrent) break;
                    include(current);
                    include(command.points[0]);
                    include(command.points[1]);
                    quadRoots(current.x(), command.points[0].x(),
                              command.points[1].x(), [&](double t) {
                                  include(quadPoint(current, command.points[0],
                                                    command.points[1], t));
                              });
                    quadRoots(current.y(), command.points[0].y(),
                              command.points[1].y(), [&](double t) {
                                  include(quadPoint(current, command.points[0],
                                                    command.points[1], t));
                              });
                    current = command.points[1];
                    break;
                case PathCommandType::CubicTo:
                    if (!hasCurrent) break;
                    include(current);
                    include(command.points[2]);
                    cubicRoots(current.x(), command.points[0].x(),
                               command.points[1].x(), command.points[2].x(),
                               [&](double t) {
                                   include(cubicPoint(current, command.points[0],
                                                      command.points[1],
                                                      command.points[2], t));
                               });
                    cubicRoots(current.y(), command.points[0].y(),
                               command.points[1].y(), command.points[2].y(),
                               [&](double t) {
                                   include(cubicPoint(current, command.points[0],
                                                      command.points[1],
                                                      command.points[2], t));
                               });
                    current = command.points[2];
                    break;
                case PathCommandType::Close:
                    hasCurrent = false;
                    break;
            }
        }

        return hasPoint ? QRectF(QPointF(minX, minY), QPointF(maxX, maxY)) : QRectF();
    }

    // 現在のパスを QPainterPath に変換
    QPainterPath toPainterPath() const {
        QPainterPath path;
        path.setFillRule(fillRule_ == PathFillRule::EvenOdd
                             ? Qt::OddEvenFill : Qt::WindingFill);
        if (commands_.empty()) return path;

        QPointF current(0, 0);
        bool hasCurrent = false;

        for (const auto& cmd : commands_) {
            switch (cmd.type) {
                case PathCommandType::MoveTo:
                    path.moveTo(cmd.points[0]);
                    current = cmd.points[0];
                    hasCurrent = true;
                    break;
                case PathCommandType::LineTo:
                    path.lineTo(cmd.points[0]);
                    current = cmd.points[0];
                    break;
                case PathCommandType::CubicTo:
                    path.cubicTo(cmd.points[0], cmd.points[1], cmd.points[2]);
                    current = cmd.points[2];
                    break;
                case PathCommandType::QuadTo:
                    path.quadTo(cmd.points[0], cmd.points[1]);
                    current = cmd.points[1];
                    break;
                case PathCommandType::Close:
                    path.closeSubpath();
                    hasCurrent = false;
                    break;
            }
        }
        return path;
    }

    // QPainterPath から ShapePath を構築
    static ShapePath fromPainterPath(const QPainterPath& path) {
        ShapePath result;
        result.impl_->fillRule_ = path.fillRule() == Qt::OddEvenFill
            ? PathFillRule::EvenOdd : PathFillRule::Winding;
        const int count = path.elementCount();
        if (count == 0) return result;

        QPointF currentPos;
        QPointF subpathStart;
        bool hasCurrent = false;

        for (int i = 0; i < count; ++i) {
            const QPainterPath::Element& e = path.elementAt(i);
            QPointF pt(e.x, e.y);

            switch (e.type) {
                case QPainterPath::MoveToElement:
                    if (hasCurrent && qFuzzyCompare(currentPos.x(), subpathStart.x()) && qFuzzyCompare(currentPos.y(), subpathStart.y())) {
                        result.impl_->commands_.push_back(PathCommand{PathCommandType::Close});
                    }
                    result.impl_->commands_.push_back(PathCommand{PathCommandType::MoveTo, pt});
                    currentPos = pt;
                    subpathStart = pt;
                    hasCurrent = true;
                    break;
                case QPainterPath::LineToElement:
                    result.impl_->commands_.push_back(PathCommand{PathCommandType::LineTo, pt});
                    currentPos = pt;
                    break;
                case QPainterPath::CurveToElement: {
                    if (i + 2 < count) {
                        const QPainterPath::Element& e1 = path.elementAt(i + 1);
                        const QPainterPath::Element& e2 = path.elementAt(i + 2);
                        QPointF cp1(e.x, e.y);
                        QPointF cp2(e1.x, e1.y);
                        QPointF end(e2.x, e2.y);
                        result.impl_->commands_.push_back(PathCommand{PathCommandType::CubicTo, cp1, cp2, end});
                        currentPos = end;
                        i += 2;
                    }
                    break;
                }
                case QPainterPath::CurveToDataElement:
                    // 単独では使用されない
                    break;
            }
        }
        if (hasCurrent && qFuzzyCompare(currentPos.x(), subpathStart.x()) && qFuzzyCompare(currentPos.y(), subpathStart.y())) {
            result.impl_->commands_.push_back(PathCommand{PathCommandType::Close});
        }
        return result;
    }

    void transform(const QTransform& matrix) {
        for (auto& cmd : commands_) {
            for (int i = 0; i < 3; ++i) {
                cmd.points[i] = matrix.map(cmd.points[i]);
            }
        }
        invalidate();
    }
};

// ========================================
// ShapePath 実装
// ========================================

ShapePath::ShapePath() : impl_(new Impl()) {}

ShapePath::~ShapePath() { delete impl_; }

ShapePath::ShapePath(const ShapePath& other) : impl_(new Impl(*other.impl_)) {}

ShapePath& ShapePath::operator=(const ShapePath& other) {
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

ShapePath::ShapePath(ShapePath&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

ShapePath& ShapePath::operator=(ShapePath&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

// ========================================
// パス構築
// ========================================

void ShapePath::clear() {
    impl_->commands_.clear();
    impl_->invalidate();
}

void ShapePath::moveTo(const QPointF& point) {
    if (!std::isfinite(point.x()) || !std::isfinite(point.y())) return;
    impl_->commands_.push_back(PathCommand{PathCommandType::MoveTo, point});
    impl_->invalidate();
}

void ShapePath::moveTo(double x, double y) {
    moveTo(QPointF(x, y));
}

void ShapePath::lineTo(const QPointF& point) {
    if (!std::isfinite(point.x()) || !std::isfinite(point.y())) return;
    impl_->commands_.push_back(PathCommand{PathCommandType::LineTo, point});
    impl_->invalidate();
}

void ShapePath::lineTo(double x, double y) {
    lineTo(QPointF(x, y));
}

void ShapePath::cubicTo(const QPointF& control1, const QPointF& control2, const QPointF& end) {
    if (!std::isfinite(control1.x()) || !std::isfinite(control1.y()) ||
        !std::isfinite(control2.x()) || !std::isfinite(control2.y()) ||
        !std::isfinite(end.x()) || !std::isfinite(end.y())) return;
    impl_->commands_.push_back(PathCommand{PathCommandType::CubicTo, control1, control2, end});
    impl_->invalidate();
}

void ShapePath::cubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey) {
    cubicTo(QPointF(c1x, c1y), QPointF(c2x, c2y), QPointF(ex, ey));
}

void ShapePath::quadTo(const QPointF& control, const QPointF& end) {
    if (!std::isfinite(control.x()) || !std::isfinite(control.y()) ||
        !std::isfinite(end.x()) || !std::isfinite(end.y())) return;
    impl_->commands_.push_back(PathCommand{PathCommandType::QuadTo, control, end});
    impl_->invalidate();
}

void ShapePath::quadTo(double cx, double cy, double ex, double ey) {
    quadTo(QPointF(cx, cy), QPointF(ex, ey));
}

void ShapePath::close() {
    impl_->commands_.push_back(PathCommand{PathCommandType::Close});
    impl_->invalidate();
}

void ShapePath::arcTo(const QRectF& rect, double startAngle, double sweepAngle) {
    if (!std::isfinite(startAngle) || !std::isfinite(sweepAngle) ||
        !std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
        !std::isfinite(rect.width()) || !std::isfinite(rect.height()) ||
        rect.width() == 0.0 || rect.height() == 0.0 || sweepAngle == 0.0) {
        return;
    }

    const QRectF normalized = rect.normalized();
    const double cx = normalized.center().x();
    const double cy = normalized.center().y();
    const double rx = normalized.width() / 2.0;
    const double ry = normalized.height() / 2.0;
    const double requestedSegments = std::ceil(std::abs(sweepAngle) / 90.0);
    const int segmentCount = requestedSegments >= 4096.0
        ? 4096
        : std::max(1, static_cast<int>(requestedSegments));
    const double delta = sweepAngle / segmentCount;
    const double radiansPerDegree = std::numbers::pi / 180.0;

    const auto pointAt = [&](double degrees) {
        const double radians = degrees * radiansPerDegree;
        return QPointF(cx + rx * std::cos(radians),
                       cy - ry * std::sin(radians));
    };
    const auto tangentAt = [&](double degrees) {
        const double radians = degrees * radiansPerDegree;
        return QPointF(-rx * std::sin(radians),
                       -ry * std::cos(radians));
    };

    const QPointF first = pointAt(startAngle);
    if (impl_->commands_.empty()) {
        moveTo(first);
    } else {
        const auto& last = impl_->commands_.back();
        if (last.type == PathCommandType::Close) {
            moveTo(first);
        } else {
            const QPointF current = last.type == PathCommandType::CubicTo
                ? last.points[2]
                : last.type == PathCommandType::QuadTo ? last.points[1] : last.points[0];
            if (current != first) lineTo(first);
        }
    }

    for (int i = 0; i < segmentCount; ++i) {
        const double a0 = startAngle + delta * i;
        const double a1 = a0 + delta;
        const double radians = delta * radiansPerDegree;
        const double factor = 4.0 / 3.0 * std::tan(radians / 4.0);
        const QPointF p0 = pointAt(a0);
        const QPointF p1 = pointAt(a1);
        const QPointF t0 = tangentAt(a0);
        const QPointF t1 = tangentAt(a1);
        cubicTo(p0 + t0 * factor, p1 - t1 * factor, p1);
    }
}

// ========================================
// 図形プリミティブ
// ========================================

void ShapePath::setRectangle(const QRectF& rect) {
    clear();
    if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
        !std::isfinite(rect.width()) || !std::isfinite(rect.height())) return;
    const QRectF normalized = rect.normalized();
    if (normalized.width() <= 0.0 || normalized.height() <= 0.0) return;
    moveTo(normalized.topLeft());
    lineTo(normalized.topRight());
    lineTo(normalized.bottomRight());
    lineTo(normalized.bottomLeft());
    close();
}

void ShapePath::setRectangle(double x, double y, double width, double height) {
    setRectangle(QRectF(x, y, width, height));
}

void ShapePath::setRoundedRect(const QRectF& rect, double radiusX, double radiusY) {
    clear();
    if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
        !std::isfinite(rect.width()) || !std::isfinite(rect.height()) ||
        !std::isfinite(radiusX) || !std::isfinite(radiusY)) return;

    const QRectF normalized = rect.normalized();
    const double left = normalized.left();
    const double top = normalized.top();
    const double right = normalized.right();
    const double bottom = normalized.bottom();
    const double rx = std::clamp(std::abs(radiusX), 0.0, normalized.width() / 2.0);
    const double ry = std::clamp(std::abs(radiusY), 0.0, normalized.height() / 2.0);
    if (rx <= 0.0 || ry <= 0.0) {
        setRectangle(rect);
        return;
    }

    moveTo(left + rx, top);
    lineTo(right - rx, top);
    arcTo(QRectF(right - 2.0 * rx, top, 2.0 * rx, 2.0 * ry), 90.0, -90.0);
    lineTo(right, bottom - ry);
    arcTo(QRectF(right - 2.0 * rx, bottom - 2.0 * ry,
                 2.0 * rx, 2.0 * ry), 0.0, -90.0);
    lineTo(left + rx, bottom);
    arcTo(QRectF(left, bottom - 2.0 * ry, 2.0 * rx, 2.0 * ry), -90.0, -90.0);
    lineTo(left, top + ry);
    arcTo(QRectF(left, top, 2.0 * rx, 2.0 * ry), 180.0, -90.0);
    close();
}

void ShapePath::setEllipse(const QRectF& rect) {
    clear();
    if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
        !std::isfinite(rect.width()) || !std::isfinite(rect.height())) return;
    const QRectF normalized = rect.normalized();
    const double cx = normalized.center().x();
    const double cy = normalized.center().y();
    const double rx = normalized.width() / 2.0;
    const double ry = normalized.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) return;

    // 楕円のベジェ近似（Kenneth I. Joy の近似係数）
    const double k = 0.5522847498;
    const double ox = rx * k;
    const double oy = ry * k;

    moveTo(cx + rx, cy);
    cubicTo(cx + rx, cy + oy, cx + ox, cy + ry, cx, cy + ry);
    cubicTo(cx - ox, cy + ry, cx - rx, cy + oy, cx - rx, cy);
    cubicTo(cx - rx, cy - oy, cx - ox, cy - ry, cx, cy - ry);
    cubicTo(cx + ox, cy - ry, cx + rx, cy - oy, cx + rx, cy);
    close();
}

void ShapePath::setEllipse(double cx, double cy, double rx, double ry) {
    setEllipse(QRectF(cx - rx, cy - ry, 2 * rx, 2 * ry));
}

void ShapePath::setPolygon(const std::vector<QPointF>& points, bool closed) {
    clear();
    if (points.size() < 2) return;

    NamedVector<QPointF> finitePoints;
    finitePoints.reserve(points.size());
    for (const auto& point : points) {
        if (std::isfinite(point.x()) && std::isfinite(point.y())) {
            finitePoints.push_back(point);
        }
    }
    if (finitePoints.size() < 2) return;

    moveTo(finitePoints[0]);
    for (size_t i = 1; i < finitePoints.size(); ++i) {
        lineTo(finitePoints[i]);
    }
    if (closed) close();
}

void ShapePath::setStar(const QPointF& center, int points, double outerRadius, double innerRadius) {
    clear();
    if (points < 2 || !std::isfinite(center.x()) || !std::isfinite(center.y()) ||
        !std::isfinite(outerRadius) || !std::isfinite(innerRadius) ||
        outerRadius <= 0.0) return;

    points = std::min(points, 4096);
    outerRadius = std::abs(outerRadius);
    innerRadius = std::clamp(std::abs(innerRadius), 0.0, outerRadius);

    const double angleStep = std::numbers::pi / points;
    const double startAngle = -std::numbers::pi / 2;

    NamedVector<QPointF> starPoints;
    starPoints.reserve(points * 2);

    for (int i = 0; i < points; ++i) {
        double angleOuter = startAngle + i * 2 * angleStep;
        double angleInner = startAngle + (i + 0.5) * 2 * angleStep;

        starPoints.push_back(QPointF(
            center.x() + outerRadius * std::cos(angleOuter),
            center.y() + outerRadius * std::sin(angleOuter)
        ));
        starPoints.push_back(QPointF(
            center.x() + innerRadius * std::cos(angleInner),
            center.y() + innerRadius * std::sin(angleInner)
        ));
    }

    setPolygon(starPoints.toStdVector(), true);
}

// ========================================
// プロパティ
// ========================================

QString ShapePath::name() const {
    return impl_->name_;
}

void ShapePath::setName(const QString& name) {
    impl_->name_ = name;
}

bool ShapePath::isClosed() const {
    if (impl_->commands_.empty()) return false;
    return impl_->commands_.back().type == PathCommandType::Close;
}

void ShapePath::setClosed(bool closed) {
    if (isClosed() == closed) return;

    if (closed) {
        if (impl_->commands_.empty() ||
            impl_->commands_.back().type == PathCommandType::MoveTo) return;
        close();
    } else {
        if (!impl_->commands_.empty() && impl_->commands_.back().type == PathCommandType::Close) {
            impl_->commands_.pop_back();
            impl_->invalidate();
        }
    }
}

double ShapePath::opacity() const {
    return impl_->opacity_;
}

void ShapePath::setOpacity(double opacity) {
    impl_->opacity_ = std::isfinite(opacity)
        ? std::clamp(opacity, 0.0, 1.0)
        : 1.0;
}

PathFillRule ShapePath::fillRule() const {
    return impl_->fillRule_;
}

void ShapePath::setFillRule(PathFillRule rule) {
    impl_->fillRule_ = rule == PathFillRule::EvenOdd
        ? PathFillRule::EvenOdd : PathFillRule::Winding;
}

bool ShapePath::isEmpty() const {
    return impl_->commands_.empty();
}

int ShapePath::commandCount() const {
    return static_cast<int>(impl_->commands_.size());
}

const std::vector<PathCommand>& ShapePath::commands() const {
    return impl_->commands_;
}

// ========================================
// ジオメトリ
// ========================================

QRectF ShapePath::boundingRect() const {
    if (impl_->dirty_) {
        impl_->cachedBounds_ = impl_->computeBounds();
        impl_->dirty_ = false;
    }
    return impl_->cachedBounds_;
}

bool ShapePath::contains(const QPointF& point) const {
    if (!std::isfinite(point.x()) || !std::isfinite(point.y())) return false;

    int winding = 0;
    constexpr double boundaryEpsilon = 1e-9;
    NamedVector<BezierSegment> fillSegments;
    for (auto segments : flattenSubpaths()) {
        if (segments.empty()) continue;
        const QPointF first = segments.front().p0;
        const QPointF last = segments.back().p1;
        if (first != last) {
            segments.push_back(BezierSegment{last, last, first, first});
        }
        for (const auto& segment : segments) {
            fillSegments.append(segment);
        }
    }

    for (const auto& segment : fillSegments) {
        const QPointF a = segment.p0;
        const QPointF b = segment.p1;
        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();
        const double cross = dx * (point.y() - a.y()) -
                             dy * (point.x() - a.x());
        const double projection = (point.x() - a.x()) * dx +
                                  (point.y() - a.y()) * dy;
        const double lengthSquared = dx * dx + dy * dy;
        if (lengthSquared > 0.0 && std::abs(cross) <= boundaryEpsilon &&
            projection >= 0.0 && projection <= lengthSquared) {
            return true;
        }

        if (a.y() <= point.y()) {
            if (b.y() > point.y() && cross > 0.0) ++winding;
        } else if (b.y() <= point.y() && cross < 0.0) {
            --winding;
        }
    }
    return fillRule() == PathFillRule::EvenOdd ? (winding % 2 != 0) : winding != 0;
}

QPointF ShapePath::pointAtPercent(double t) const {
    const double total = length();
    if (total <= 0.0) {
        const auto segments = flatten();
        return segments.empty() ? QPointF() : segments.front().p0;
    }
    return pointAtLength(std::clamp(t, 0.0, 1.0) * total);
}

double ShapePath::length() const {
    double total = 0.0;
    for (const auto& segment : flatten()) {
        total += std::hypot(segment.p1.x() - segment.p0.x(),
                            segment.p1.y() - segment.p0.y());
    }
    return total;
}

QPointF ShapePath::pointAtLength(double length) const {
    const auto segments = flatten();
    if (segments.empty()) return QPointF();

    double total = 0.0;
    for (const auto& segment : segments) {
        total += std::hypot(segment.p1.x() - segment.p0.x(),
                            segment.p1.y() - segment.p0.y());
    }
    if (total <= 0.0) return segments.front().p0;

    double remaining = std::clamp(length, 0.0, total);
    for (const auto& segment : segments) {
        const double dx = segment.p1.x() - segment.p0.x();
        const double dy = segment.p1.y() - segment.p0.y();
        const double segmentLength = std::hypot(dx, dy);
        if (segmentLength <= 0.0) continue;
        if (remaining <= segmentLength) {
            const double ratio = remaining / segmentLength;
            return segment.p0 + (segment.p1 - segment.p0) * ratio;
        }
        remaining -= segmentLength;
    }
    return segments.back().p1;
}

std::vector<BezierSegment> ShapePath::toSegments() const {
    NamedVector<BezierSegment> segments;
    const auto& cmds = impl_->commands_;
    if (cmds.empty()) return {};

    QPointF currentPos;  // 現在のパス位置
    QPointF subpathStart; // 現在のサブパス始点
    bool hasCurrent = false;
    const auto finite = [](const QPointF& point) {
        return std::isfinite(point.x()) && std::isfinite(point.y());
    };

    for (size_t i = 0; i < cmds.size(); ++i) {
        const auto& cmd = cmds[i];
        switch (cmd.type) {
            case PathCommandType::MoveTo:
                if (!finite(cmd.points[0])) {
                    hasCurrent = false;
                    break;
                }
                currentPos = cmd.points[0];
                subpathStart = currentPos;
                hasCurrent = true;
                break;
            case PathCommandType::LineTo: {
                QPointF end = cmd.points[0];
                if (!hasCurrent || !finite(end)) break;
                QPointF start = currentPos;
                segments.push_back(BezierSegment{start, start, end, end});
                currentPos = end;
                break;
            }
            case PathCommandType::CubicTo: {
                if (!hasCurrent || !finite(cmd.points[0]) ||
                    !finite(cmd.points[1]) || !finite(cmd.points[2])) break;
                QPointF start = currentPos;
                segments.push_back(BezierSegment{start, cmd.points[0], cmd.points[1], cmd.points[2]});
                currentPos = cmd.points[2];
                break;
            }
            case PathCommandType::QuadTo: {
                if (!hasCurrent || !finite(cmd.points[0]) || !finite(cmd.points[1])) break;
                QPointF start = currentPos;
                QPointF cp = cmd.points[0];
                QPointF end = cmd.points[1];
                QPointF cp1 = start + 2.0 / 3.0 * (cp - start);
                QPointF cp2 = end + 2.0 / 3.0 * (cp - end);
                segments.push_back(BezierSegment{start, cp1, cp2, end});
                currentPos = end;
                break;
            }
            case PathCommandType::Close:
                if (hasCurrent && currentPos != subpathStart) {
                    segments.push_back(BezierSegment{
                        currentPos, currentPos, subpathStart, subpathStart});
                }
                currentPos = subpathStart;
                hasCurrent = false;
                break;
        }
    }
    return segments.toStdVector();
}

std::vector<BezierSegment> ShapePath::flatten(double tolerance) const {
    NamedVector<BezierSegment> flattened;
    if (!std::isfinite(tolerance) || tolerance <= 0.0) tolerance = 0.25;
    tolerance = std::clamp(tolerance, 1e-6, 1e6);

    const auto distanceToChord = [](const QPointF& point,
                                    const QPointF& start,
                                    const QPointF& end) {
        const double dx = end.x() - start.x();
        const double dy = end.y() - start.y();
        const double lengthSquared = dx * dx + dy * dy;
        if (lengthSquared <= std::numeric_limits<double>::epsilon()) {
            return std::hypot(point.x() - start.x(), point.y() - start.y());
        }
        const double cross = dx * (point.y() - start.y()) -
                             dy * (point.x() - start.x());
        return std::abs(cross) / std::sqrt(lengthSquared);
    };

    const auto appendLine = [&](const QPointF& start, const QPointF& end) {
        if (std::isfinite(start.x()) && std::isfinite(start.y()) &&
            std::isfinite(end.x()) && std::isfinite(end.y())) {
            flattened.push_back(BezierSegment{start, start, end, end});
        }
    };

    const auto flattenSegment = [&](const BezierSegment& segment) {
        std::function<void(const BezierSegment&, int)> subdivide;
        subdivide = [&](const BezierSegment& current, int depth) {
            const double error = std::max(
                distanceToChord(current.cp1, current.p0, current.p1),
                distanceToChord(current.cp2, current.p0, current.p1));
            if (error <= tolerance || depth >= 16) {
                appendLine(current.p0, current.p1);
                return;
            }

            const QPointF p01 = (current.p0 + current.cp1) * 0.5;
            const QPointF p12 = (current.cp1 + current.cp2) * 0.5;
            const QPointF p23 = (current.cp2 + current.p1) * 0.5;
            const QPointF p012 = (p01 + p12) * 0.5;
            const QPointF p123 = (p12 + p23) * 0.5;
            const QPointF midpoint = (p012 + p123) * 0.5;

            subdivide(BezierSegment{current.p0, p01, p012, midpoint}, depth + 1);
            subdivide(BezierSegment{midpoint, p123, p23, current.p1}, depth + 1);
        };
        subdivide(segment, 0);
    };

    for (const auto& segment : toSegments()) flattenSegment(segment);
    return flattened.toStdVector();
}

std::vector<std::vector<BezierSegment>> ShapePath::flattenSubpaths(double tolerance) const {
    NamedVector<std::vector<BezierSegment>> result;
    for (const auto& subpath : subpaths()) {
        auto flattened = subpath.flatten(tolerance);
        if (!flattened.empty()) result.push_back(std::move(flattened));
    }
    return result.toStdVector();
}

std::vector<PathTriangle> ShapePath::triangulateSimple(double tolerance) const {
    NamedVector<PathTriangle> triangles;
    const auto subpaths = flattenSubpaths(tolerance);
    if (subpaths.size() != 1 || subpaths.front().size() < 3 || !isClosed()) return {};

    std::vector<QPointF> polygon;
    polygon.reserve(subpaths.front().size());
    for (const auto& segment : subpaths.front()) polygon.push_back(segment.p0);
    if (polygon.size() >= 2 && polygon.front() == polygon.back()) polygon.pop_back();
    if (polygon.size() < 3) return {};

    const auto cross = [](const QPointF& a, const QPointF& b, const QPointF& c) {
        return (b.x() - a.x()) * (c.y() - a.y()) -
               (b.y() - a.y()) * (c.x() - a.x());
    };
    double signedArea = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        signedArea += a.x() * b.y() - b.x() * a.y();
    }
    if (std::abs(signedArea) <= 1e-9) return {};

    std::vector<size_t> indices(polygon.size());
    std::iota(indices.begin(), indices.end(), 0);
    const bool ccw = signedArea > 0.0;
    const auto inside = [&](const QPointF& point, const QPointF& a,
                            const QPointF& b, const QPointF& c) {
        const double c0 = cross(a, b, point);
        const double c1 = cross(b, c, point);
        const double c2 = cross(c, a, point);
        return ccw ? c0 >= -1e-9 && c1 >= -1e-9 && c2 >= -1e-9
                   : c0 <= 1e-9 && c1 <= 1e-9 && c2 <= 1e-9;
    };

    size_t guard = 0;
    while (indices.size() > 2 && guard++ < polygon.size() * polygon.size()) {
        bool clipped = false;
        for (size_t i = 0; i < indices.size(); ++i) {
            const size_t prev = indices[(i + indices.size() - 1) % indices.size()];
            const size_t curr = indices[i];
            const size_t next = indices[(i + 1) % indices.size()];
            const double turn = cross(polygon[prev], polygon[curr], polygon[next]);
            if ((ccw && turn <= 1e-9) || (!ccw && turn >= -1e-9)) continue;
            bool containsVertex = false;
            for (const size_t candidate : indices) {
                if (candidate == prev || candidate == curr || candidate == next) continue;
                if (inside(polygon[candidate], polygon[prev], polygon[curr], polygon[next])) {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex) continue;
            triangles.push_back(PathTriangle{polygon[prev], polygon[curr], polygon[next]});
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) return {};
    }
    return triangles.toStdVector();
}

namespace {

struct RayCrossings {
    int winding = 0;
    int crossings = 0;
};

// 点から +x 方向のレイと閉輪郭の交差を数える（winding と交差回数）。
RayCrossings contourRayCrossings(const std::vector<QPointF>& contour,
                                 const QPointF& point) {
    RayCrossings result;
    const size_t count = contour.size();
    for (size_t i = 0; i < count; ++i) {
        const QPointF& a = contour[i];
        const QPointF& b = contour[(i + 1) % count];
        const double cross = (b.x() - a.x()) * (point.y() - a.y()) -
                             (b.y() - a.y()) * (point.x() - a.x());
        if (a.y() <= point.y()) {
            if (b.y() > point.y() && cross > 0.0) {
                ++result.winding;
                ++result.crossings;
            }
        } else if (b.y() <= point.y() && cross < 0.0) {
            --result.winding;
            ++result.crossings;
        }
    }
    return result;
}

bool fillRuleFilled(PathFillRule rule, const RayCrossings& hits) {
    return rule == PathFillRule::EvenOdd ? (hits.crossings % 2) != 0
                                         : hits.winding != 0;
}

double contourSignedArea(const std::vector<QPointF>& contour) {
    double area = 0.0;
    for (size_t i = 0; i < contour.size(); ++i) {
        const QPointF& a = contour[i];
        const QPointF& b = contour[(i + 1) % contour.size()];
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area * 0.5;
}

double triangleCross(const QPointF& a, const QPointF& b, const QPointF& c) {
    return (b.x() - a.x()) * (c.y() - a.y()) -
           (b.y() - a.y()) * (c.x() - a.x());
}

bool pointInTriangleInclusive(const QPointF& point, const QPointF& a,
                              const QPointF& b, const QPointF& c) {
    const double d0 = triangleCross(a, b, point);
    const double d1 = triangleCross(b, c, point);
    const double d2 = triangleCross(c, a, point);
    const bool hasNeg = d0 < 0.0 || d1 < 0.0 || d2 < 0.0;
    const bool hasPos = d0 > 0.0 || d1 > 0.0 || d2 > 0.0;
    return !(hasNeg && hasPos);
}

// 輪郭の内部点を ear の重心候補から探す。
bool contourInteriorPoint(const std::vector<QPointF>& contour,
                          QPointF* interior) {
    const size_t count = contour.size();
    for (size_t i = 0; i < count; ++i) {
        const QPointF& prev = contour[(i + count - 1) % count];
        const QPointF& curr = contour[i];
        const QPointF& next = contour[(i + 1) % count];
        const QPointF candidate((prev.x() + curr.x() + next.x()) / 3.0,
                                (prev.y() + curr.y() + next.y()) / 3.0);
        if (contourRayCrossings(contour, candidate).winding != 0) {
            *interior = candidate;
            return true;
        }
    }
    return false;
}

// CCW 前提の ear clipping。ブリッジ由来の重複頂点・共線頂点を許容する。
bool earClipContour(const std::vector<QPointF>& polygon,
                    NamedVector<PathTriangle>* triangles) {
    if (polygon.size() < 3) return false;
    std::vector<size_t> indices(polygon.size());
    std::iota(indices.begin(), indices.end(), 0);

    size_t guard = 0;
    const size_t guardLimit = polygon.size() * polygon.size() + 16;
    while (indices.size() > 2 && guard++ < guardLimit) {
        bool clipped = false;
        for (size_t i = 0; i < indices.size(); ++i) {
            const QPointF& a =
                polygon[indices[(i + indices.size() - 1) % indices.size()]];
            const QPointF& b = polygon[indices[i]];
            const QPointF& c = polygon[indices[(i + 1) % indices.size()]];
            const double turn = triangleCross(a, b, c);
            if (std::abs(turn) <= 1e-12) {
                // 退化した頂点（ブリッジの折り返し・共線）は三角形を出さずに除去。
                indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
                break;
            }
            if (turn < 0.0) continue;  // reflex（CCW 前提）
            bool containsVertex = false;
            for (const size_t candidate : indices) {
                const QPointF& point = polygon[candidate];
                if (point == a || point == b || point == c) continue;
                if (pointInTriangleInclusive(point, a, b, c)) {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex) continue;
            triangles->push_back(PathTriangle{a, b, c});
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) return false;
    }
    return indices.size() <= 2;
}

// 穴輪郭（CW）を外輪郭（CCW）へゼロ幅ブリッジで接続して 1 本の輪郭にする。
bool mergeHoleIntoOuter(std::vector<QPointF>* outer,
                        const std::vector<QPointF>& hole) {
    if (outer->size() < 3 || hole.size() < 3) return false;

    size_t holeIndex = 0;
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].x() > hole[holeIndex].x()) holeIndex = i;
    }
    const QPointF m = hole[holeIndex];

    // +x レイと外輪郭エッジの最近交点を探す。
    constexpr size_t npos = std::numeric_limits<size_t>::max();
    double bestX = std::numeric_limits<double>::max();
    size_t bestEdge = npos;
    const size_t count = outer->size();
    for (size_t i = 0; i < count; ++i) {
        const QPointF& a = (*outer)[i];
        const QPointF& b = (*outer)[(i + 1) % count];
        if ((a.y() > m.y()) == (b.y() > m.y())) continue;
        const double t = (m.y() - a.y()) / (b.y() - a.y());
        const double x = a.x() + t * (b.x() - a.x());
        if (x >= m.x() && x < bestX) {
            bestX = x;
            bestEdge = i;
        }
    }
    if (bestEdge == npos) return false;
    const QPointF intersection(bestX, m.y());

    // 初期候補は交差エッジのうち x が大きい端点。三角形 (m, I, P) 内に
    // reflex 頂点があれば、+x に最も近い角度の頂点へブリッジ先を置き換える。
    const QPointF& edgeA = (*outer)[bestEdge];
    const QPointF& edgeB = (*outer)[(bestEdge + 1) % count];
    size_t bridgeIndex =
        (edgeA.x() > edgeB.x()) ? bestEdge : (bestEdge + 1) % count;
    const QPointF initialTarget = (*outer)[bridgeIndex];

    double bestMetric = std::numeric_limits<double>::max();
    for (size_t i = 0; i < count; ++i) {
        if (i == bridgeIndex) continue;
        const QPointF& candidate = (*outer)[i];
        if (candidate.x() < m.x()) continue;
        const QPointF& prev = (*outer)[(i + count - 1) % count];
        const QPointF& next = (*outer)[(i + 1) % count];
        if (triangleCross(prev, candidate, next) >= 0.0) continue;  // 凸は対象外
        if (!pointInTriangleInclusive(candidate, m, intersection,
                                      initialTarget)) {
            continue;
        }
        const double dx = candidate.x() - m.x();
        if (dx <= 0.0) continue;
        const double metric = std::abs(candidate.y() - m.y()) / dx;
        if (metric < bestMetric) {
            bestMetric = metric;
            bridgeIndex = i;
        }
    }

    NamedVector<QPointF> merged;
    merged.reserve(outer->size() + hole.size() + 2);
    for (size_t i = 0; i <= bridgeIndex; ++i) merged.push_back((*outer)[i]);
    for (size_t i = 0; i <= hole.size(); ++i) {
        merged.push_back(hole[(holeIndex + i) % hole.size()]);
    }
    merged.push_back((*outer)[bridgeIndex]);
    for (size_t i = bridgeIndex + 1; i < count; ++i) {
        merged.push_back((*outer)[i]);
    }
    *outer = merged.toStdVector();
    return true;
}

}  // namespace

std::vector<PathTriangle> ShapePath::triangulate(double tolerance) const {
    struct FillContour {
        std::vector<QPointF> points;
        double area = 0.0;
        QPointF interior;
        double maxX = 0.0;
    };

    NamedVector<PathTriangle> triangles;
    const auto flattenedSubpaths = flattenSubpaths(tolerance);
    if (flattenedSubpaths.empty()) return {};

    // fill は contains() と同じく、閉じていないサブパスも暗黙に閉じて扱う。
    std::vector<FillContour> contours;
    contours.reserve(flattenedSubpaths.size());
    for (const auto& segments : flattenedSubpaths) {
        FillContour contour;
        contour.points.reserve(segments.size());
        for (const auto& segment : segments) contour.points.push_back(segment.p0);
        if (!segments.empty() && segments.back().p1 != segments.front().p0) {
            contour.points.push_back(segments.back().p1);
        }
        if (contour.points.size() >= 2 &&
            contour.points.front() == contour.points.back()) {
            contour.points.pop_back();
        }
        if (contour.points.size() < 3) continue;
        contour.area = contourSignedArea(contour.points);
        if (std::abs(contour.area) <= 1e-9) continue;
        if (!contourInteriorPoint(contour.points, &contour.interior)) continue;
        contour.maxX = contour.points.front().x();
        for (const auto& point : contour.points) {
            contour.maxX = std::max(contour.maxX, point.x());
        }
        contours.push_back(std::move(contour));
    }
    if (contours.empty()) return {};

    // fill rule に基づいて外輪郭／穴／冗長輪郭を分類する。
    const PathFillRule rule = fillRule();
    NamedVector<size_t> outers;
    NamedVector<size_t> holes;
    for (size_t i = 0; i < contours.size(); ++i) {
        RayCrossings total;
        RayCrossings own;
        for (size_t j = 0; j < contours.size(); ++j) {
            const RayCrossings hits =
                contourRayCrossings(contours[j].points, contours[i].interior);
            total.winding += hits.winding;
            total.crossings += hits.crossings;
            if (j == i) own = hits;
        }
        const bool filledInside = fillRuleFilled(rule, total);
        const RayCrossings outside{total.winding - own.winding,
                                   total.crossings - own.crossings};
        const bool filledOutside = fillRuleFilled(rule, outside);
        if (filledInside && !filledOutside) {
            outers.push_back(i);
        } else if (!filledInside && filledOutside) {
            holes.push_back(i);
        }
        // 両側 filled（冗長輪郭）と両側 unfilled は描画へ寄与しない。
    }
    if (outers.empty()) return {};

    // 穴を最も内側（最小面積）の外輪郭へ割り当てる。
    std::vector<std::vector<size_t>> holesByOuter(outers.size());
    for (const size_t holeIndex : holes) {
        size_t parent = std::numeric_limits<size_t>::max();
        double parentArea = std::numeric_limits<double>::max();
        for (size_t o = 0; o < outers.size(); ++o) {
            const FillContour& outer = contours[outers[o]];
            if (contourRayCrossings(outer.points,
                                    contours[holeIndex].interior).winding == 0) {
                continue;
            }
            const double area = std::abs(outer.area);
            if (area < parentArea) {
                parentArea = area;
                parent = o;
            }
        }
        if (parent == std::numeric_limits<size_t>::max()) continue;
        holesByOuter[parent].push_back(holeIndex);
    }

    for (size_t o = 0; o < outers.size(); ++o) {
        FillContour& outer = contours[outers[o]];
        std::vector<QPointF> polygon = outer.points;
        if (outer.area < 0.0) std::reverse(polygon.begin(), polygon.end());

        // ブリッジ同士の交差を避けるため、穴は最大 x の降順で接続する。
        std::vector<size_t>& holeIndices = holesByOuter[o];
        std::sort(holeIndices.begin(), holeIndices.end(),
                  [&](size_t lhs, size_t rhs) {
                      return contours[lhs].maxX > contours[rhs].maxX;
                  });
        for (const size_t holeIndex : holeIndices) {
            std::vector<QPointF> holePolygon = contours[holeIndex].points;
            if (contours[holeIndex].area > 0.0) {
                std::reverse(holePolygon.begin(), holePolygon.end());
            }
            if (!mergeHoleIntoOuter(&polygon, holePolygon)) return {};
        }

        if (!earClipContour(polygon, &triangles)) return {};
    }
    return triangles.toStdVector();
}

// ========================================
// 変換
// ========================================

void ShapePath::translate(const QPointF& offset) {
    impl_->transform(QTransform::fromTranslate(offset.x(), offset.y()));
}

void ShapePath::scale(const QPointF& center, double sx, double sy) {
    QTransform t;
    t.translate(center.x(), center.y());
    t.scale(sx, sy);
    t.translate(-center.x(), -center.y());
    impl_->transform(t);
}

void ShapePath::rotate(const QPointF& center, double angle) {
    QTransform t;
    t.translate(center.x(), center.y());
    t.rotate(angle);
    t.translate(-center.x(), -center.y());
    impl_->transform(t);
}

void ShapePath::transform(const QTransform& matrix) {
    impl_->transform(matrix);
}

// ========================================
// Qt連携
// ========================================

QPainterPath ShapePath::toPainterPath() const {
    return impl_->toPainterPath();
}

ShapePath ShapePath::fromPainterPath(const QPainterPath& path) {
    return Impl::fromPainterPath(path);
}

// ========================================
// ユーティリティ
// ========================================

ShapePath ShapePath::clone() const {
    return ShapePath(*this);
}

void ShapePath::reverse() {
    NamedVector<PathCommand> reversed;
    reversed.reserve(impl_->commands_.size());

    struct Segment {
        PathCommandType type;
        QPointF start;
        QPointF end;
        QPointF control1;
        QPointF control2;
    };

    const auto reverseSubpath = [&reversed](const std::vector<PathCommand>& commands) {
        if (commands.empty() || commands.front().type != PathCommandType::MoveTo) return;

        QPointF current = commands.front().points[0];
        std::vector<Segment> segments;
        bool closed = false;
        for (size_t i = 1; i < commands.size(); ++i) {
            const auto& command = commands[i];
            if (command.type == PathCommandType::Close) {
                closed = true;
                continue;
            }
            Segment segment{command.type, current, current, {}, {}};
            switch (command.type) {
                case PathCommandType::LineTo:
                    segment.end = command.points[0];
                    break;
                case PathCommandType::QuadTo:
                    segment.control1 = command.points[0];
                    segment.end = command.points[1];
                    break;
                case PathCommandType::CubicTo:
                    segment.control1 = command.points[0];
                    segment.control2 = command.points[1];
                    segment.end = command.points[2];
                    break;
                case PathCommandType::MoveTo:
                case PathCommandType::Close:
                    continue;
            }
            segments.push_back(segment);
            current = segment.end;
        }
        if (segments.empty()) return;

        reversed.push_back(PathCommand{PathCommandType::MoveTo, segments.back().end});
        for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
            switch (it->type) {
                case PathCommandType::LineTo:
                    reversed.push_back(PathCommand{PathCommandType::LineTo, it->start});
                    break;
                case PathCommandType::QuadTo:
                    reversed.push_back(PathCommand{PathCommandType::QuadTo,
                                                   it->control1, it->start});
                    break;
                case PathCommandType::CubicTo:
                    reversed.push_back(PathCommand{PathCommandType::CubicTo,
                                                   it->control2, it->control1,
                                                   it->start});
                    break;
                case PathCommandType::MoveTo:
                case PathCommandType::Close:
                    break;
            }
        }
        if (closed) reversed.push_back(PathCommand{PathCommandType::Close});
    };

    std::vector<PathCommand> subpath;
    for (const auto& command : impl_->commands_) {
        if (command.type == PathCommandType::MoveTo && !subpath.empty()) {
            reverseSubpath(subpath);
            subpath.clear();
        }
        subpath.push_back(command);
    }
    reverseSubpath(subpath);
    impl_->commands_ = reversed.toStdVector();
    impl_->invalidate();
}

void ShapePath::addPath(const ShapePath& other) {
    if (other.isEmpty()) return;
    if (this == &other) {
        const auto commands = other.impl_->commands_;
        impl_->commands_.insert(impl_->commands_.end(), commands.begin(), commands.end());
    } else {
        impl_->commands_.insert(impl_->commands_.end(),
                                other.impl_->commands_.begin(),
                                other.impl_->commands_.end());
    }
    impl_->invalidate();
}

void ShapePath::simplify() {
    if (impl_->commands_.empty()) return;
    QPainterPath path = toPainterPath();
    QPainterPath simplified = path.simplified();
    *this = fromPainterPath(simplified);
}

// ========================================
// ヘルパー関数（private）
// ========================================

QPointF ShapePath::getStartPoint(const PathCommand& cmd) const {
    switch (cmd.type) {
        case PathCommandType::MoveTo:
        case PathCommandType::LineTo:
            return cmd.points[0];
        case PathCommandType::CubicTo:
            return cmd.points[0];
        case PathCommandType::QuadTo:
            return cmd.points[0];
        case PathCommandType::Close:
            return QPointF();
    }
    return QPointF();
}

QPointF ShapePath::getEndPoint(const PathCommand& cmd) const {
    switch (cmd.type) {
        case PathCommandType::MoveTo:
        case PathCommandType::LineTo:
            return cmd.points[0];
        case PathCommandType::CubicTo:
            return cmd.points[2];
        case PathCommandType::QuadTo:
            return cmd.points[1];
        case PathCommandType::Close:
            return QPointF();
    }
    return QPointF();
}

double ShapePath::cubicApproxLength(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3) const {
    constexpr int N = 5;
    double total = 0.0;
    QPointF prev = p0;

    for (int i = 1; i <= N; ++i) {
        double t = static_cast<double>(i) / N;
        double u = 1.0 - t;
        double tt = t * t;
        double uu = u * u;
        double uuu = uu * u;
        double ttt = tt * t;
        QPointF curr = uuu * p0 + 3.0 * uu * t * p1 + 3.0 * u * tt * p2 + ttt * p3;
        total += (curr - prev).manhattanLength();
        prev = curr;
    }
    return total;
}

double ShapePath::quadApproxLength(const QPointF& p0, const QPointF& p1, const QPointF& p2) const {
    constexpr int N = 5;
    double total = 0.0;
    QPointF prev = p0;

    for (int i = 1; i <= N; ++i) {
        double t = static_cast<double>(i) / N;
        double u = 1.0 - t;
        QPointF curr = u * u * p0 + 2.0 * u * t * p1 + t * t * p2;
        total += (curr - prev).manhattanLength();
        prev = curr;
    }
    return total;
}

QPointF ShapePath::cubicPointAtLength(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, double targetLen) const {
    double low = 0.0, high = 1.0;
    QPointF result = p0;

    for (int iter = 0; iter < 20; ++iter) {
        double mid = (low + high) / 2.0;
        double approxLen = mid * cubicApproxLength(p0, p1, p2, p3);
        if (approxLen < targetLen) {
            low = mid;
            double u = 1.0 - mid;
            double tt = mid * mid;
            double uu = u * u;
            double uuu = uu * u;
            double ttt = tt * mid;
            result = uuu * p0 + 3.0 * uu * mid * p1 + 3.0 * u * tt * p2 + ttt * p3;
        } else {
            high = mid;
        }
    }
    return result;
}

QPointF ShapePath::quadPointAtLength(const QPointF& p0, const QPointF& p1, const QPointF& p2, double targetLen) const {
    double low = 0.0, high = 1.0;
    QPointF result = p0;

    for (int iter = 0; iter < 20; ++iter) {
        double mid = (low + high) / 2.0;
        double approxLen = mid * quadApproxLength(p0, p1, p2);
        if (approxLen < targetLen) {
            low = mid;
            double u = 1.0 - mid;
            result = u * u * p0 + 2.0 * u * mid * p1 + mid * mid * p2;
        } else {
            high = mid;
        }
    }
    return result;
}

// ========================================
// サブパス分解
// ========================================

std::vector<ShapePath> ShapePath::subpaths() const
{
    NamedVector<ShapePath> result;
    if (impl_->commands_.empty()) return {};

    ShapePath current;
    for (const auto& cmd : impl_->commands_) {
        if (cmd.type == PathCommandType::MoveTo) {
            if (!current.impl_->commands_.empty()) {
                result.push_back(std::move(current));
                current = ShapePath();
            }
            current.impl_->commands_.push_back(cmd);
        } else {
            current.impl_->commands_.push_back(cmd);
        }
    }
    if (!current.impl_->commands_.empty())
        result.push_back(std::move(current));
    return result.toStdVector();
}

// ========================================
// 等距離サンプリング
// ========================================

std::vector<QPointF> ShapePath::sampleEquidistant(int count) const
{
    NamedVector<QPointF> result;
    if (impl_->commands_.empty() || count < 2) return {};

    const double totalLen = length();
    if (totalLen < 1e-9) {
        const auto segments = flatten();
        const QPointF point = segments.empty() ? QPointF() : segments.front().p0;
        result.resize(count, point);
        return result;
    }

    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(pointAtLength(
            static_cast<double>(i) / (count - 1) * totalLen));
    }
    return result.toStdVector();
}

// ========================================
// パス間補間
// ========================================

ShapePath ShapePath::interpolate(
    const ShapePath& from, const ShapePath& to,
    double t, int sampleCount)
{
    if (from.isEmpty() && to.isEmpty()) return {};
    if (from.isEmpty()) return to;
    if (to.isEmpty()) return from;
    if (t <= 0.0) return from;
    if (t >= 1.0) return to;

    const auto subA = from.subpaths();
    const auto subB = to.subpaths();
    const size_t n = std::min(subA.size(), subB.size());
    if (n == 0) return {};

    ShapePath result;
    for (size_t s = 0; s < n; ++s) {
        const auto ptsA = subA[s].sampleEquidistant(sampleCount);
        const auto ptsB = subB[s].sampleEquidistant(sampleCount);
        const size_t m = std::min(ptsA.size(), ptsB.size());
        if (m < 2) continue;

        const QPointF start = ptsA[0] + (ptsB[0] - ptsA[0]) * t;
        result.moveTo(start);

        for (size_t i = 1; i < m; ++i) {
            const QPointF p = ptsA[i] + (ptsB[i] - ptsA[i]) * t;
            result.lineTo(p);
        }

        if (subA[s].isClosed() || subB[s].isClosed())
            result.close();
    }
    return result;
}

// ========================================
// 面積
// ========================================

double ShapePath::area() const
{
    double totalArea = 0.0;
    for (const auto& subpath : subpaths()) {
        if (!subpath.isClosed()) continue;
        const auto sampled = subpath.sampleEquidistant(256);
        if (sampled.size() < 3) continue;

        double subpathArea = 0.0;
        for (size_t i = 0; i < sampled.size(); ++i) {
            const auto& p0 = sampled[i];
            const auto& p1 = sampled[(i + 1) % sampled.size()];
            subpathArea += p0.x() * p1.y() - p1.x() * p0.y();
        }
        totalArea += subpathArea * 0.5;
    }
    return totalArea;
}

// ========================================
// 重心
// ========================================

QPointF ShapePath::centroid() const
{
    double area2 = 0.0;
    double centroidXNumerator = 0.0;
    double centroidYNumerator = 0.0;
    for (const auto& subpath : subpaths()) {
        if (!subpath.isClosed()) continue;
        const auto sampled = subpath.sampleEquidistant(256);
        if (sampled.size() < 3) continue;
        for (size_t i = 0; i < sampled.size(); ++i) {
            const auto& p0 = sampled[i];
            const auto& p1 = sampled[(i + 1) % sampled.size()];
            const double cross = p0.x() * p1.y() - p1.x() * p0.y();
            area2 += cross;
            centroidXNumerator += (p0.x() + p1.x()) * cross;
            centroidYNumerator += (p0.y() + p1.y()) * cross;
        }
    }
    if (std::abs(area2) > 1e-12) {
        return QPointF(centroidXNumerator / (3.0 * area2),
                       centroidYNumerator / (3.0 * area2));
    }

    const auto sampled = sampleEquidistant(256);
    if (sampled.empty()) return {};
    double averageX = 0.0;
    double averageY = 0.0;
    for (const auto& point : sampled) {
        averageX += point.x();
        averageY += point.y();
    }
    return QPointF(averageX / sampled.size(), averageY / sampled.size());
}

// ========================================
// 接線・法線
// ========================================

QPointF ShapePath::tangentAtPercent(double t) const
{
    const double total = length();
    if (total <= 0.0) return {};

    t = std::clamp(t, 0.0, 1.0);
    const double epsilon = std::max(total * 0.001, 1e-6);
    const double center = t * total;
    const double before = std::max(0.0, center - epsilon);
    const double after = std::min(total, center + epsilon);
    const QPointF p0 = pointAtLength(before);
    const QPointF p1 = pointAtLength(after);
    QPointF dir = p1 - p0;

    const double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len < 1e-12) return {};
    return dir / len;
}

QPointF ShapePath::normalAtPercent(double t) const
{
    const QPointF tan = tangentAtPercent(t);
    return QPointF(-tan.y(), tan.x());
}

// ========================================
// 巻き方向判定
// ========================================

bool ShapePath::isClockwise() const
{
    return area() < 0.0;
}

// ========================================
// パスオフセット（膨張・収縮）
// ========================================

ShapePath ShapePath::offsetPath(double delta, int subdivisions) const
{
    if (std::abs(delta) < 0.001) return clone();

    auto samples = sampleEquidistant(subdivisions);
    if (samples.size() < 3) return clone();

    // 各点の法線方向に delta だけオフセット
    const int n = static_cast<int>(samples.size());
    const bool closed = isClosed();

    std::vector<QPointF> offsetPts(n);
    for (int i = 0; i < n; ++i) {
        const int prev = (i == 0) ? (closed ? n - 1 : 0) : i - 1;
        const int next = (i == n - 1) ? (closed ? 0 : n - 1) : i + 1;
        const QPointF d = samples[next] - samples[prev];

        // 法線 = 接線（d方向）を右90度回転
        const double len = std::sqrt(d.x() * d.x() + d.y() * d.y());
        if (len < 1e-12) {
            offsetPts[i] = samples[i];
            continue;
        }
        QPointF nrm(-d.y() / len, d.x() / len);
        offsetPts[i] = samples[i] + nrm * delta;
    }

    ShapePath result;
    result.moveTo(offsetPts[0]);
    for (int i = 1; i < n; ++i)
        result.lineTo(offsetPts[i]);
    if (closed) result.close();
    return result;
}

// ========================================
// シリアライズ
// ========================================

QJsonObject ShapePath::toJson() const
{
    QJsonObject obj;
    obj["name"] = name();
    obj["opacity"] = opacity();
    obj["fillRule"] = static_cast<int>(fillRule());

    QJsonArray cmdArr;
    for (const auto& cmd : impl_->commands_) {
        QJsonObject c;
        c["type"] = static_cast<int>(cmd.type);
        QJsonArray pts;
        const int pointCount = cmd.type == PathCommandType::MoveTo ||
                cmd.type == PathCommandType::LineTo ? 1 :
                cmd.type == PathCommandType::CubicTo ? 3 :
                cmd.type == PathCommandType::QuadTo ? 2 : 0;
        for (int i = 0; i < pointCount; ++i) {
            QJsonArray pt;
            pt.append(cmd.points[i].x());
            pt.append(cmd.points[i].y());
            pts.append(pt);
        }
        c["pts"] = pts;
        cmdArr.append(c);
    }
    obj["commands"] = cmdArr;
    return obj;
}

ShapePath ShapePath::fromJson(const QJsonObject& obj)
{
    ShapePath path;
    path.setName(obj["name"].toString());
    if (obj.contains("opacity")) path.setOpacity(obj["opacity"].toDouble());
    if (obj.contains("fillRule")) {
        path.setFillRule(static_cast<PathFillRule>(obj["fillRule"].toInt()));
    }

    const QJsonArray cmdArr = obj["commands"].toArray();
    for (const auto& val : cmdArr) {
        const QJsonObject c = val.toObject();
        const int type = c["type"].toInt();
        const QJsonArray pts = c["pts"].toArray();
        if (type < static_cast<int>(PathCommandType::MoveTo) ||
            type > static_cast<int>(PathCommandType::Close)) continue;

        const auto pointAt = [&pts](int index, QPointF& point) {
            if (index < 0 || index >= pts.size()) return false;
            const QJsonArray value = pts[index].toArray();
            if (value.size() < 2) return false;
            point = QPointF(value[0].toDouble(), value[1].toDouble());
            return std::isfinite(point.x()) && std::isfinite(point.y());
        };

        QPointF p0;
        QPointF p1;
        QPointF p2;
        switch (static_cast<PathCommandType>(type)) {
            case PathCommandType::MoveTo:
                if (pointAt(0, p0)) path.moveTo(p0);
                break;
            case PathCommandType::LineTo:
                if (pointAt(0, p0)) path.lineTo(p0);
                break;
            case PathCommandType::CubicTo:
                if (pointAt(0, p0) && pointAt(1, p1) && pointAt(2, p2))
                    path.cubicTo(p0, p1, p2);
                break;
            case PathCommandType::QuadTo:
                if (pointAt(0, p0) && pointAt(1, p1)) path.quadTo(p0, p1);
                break;
            case PathCommandType::Close:
                path.close();
                break;
        }
    }
    return path;
}

} // namespace ArtifactCore
