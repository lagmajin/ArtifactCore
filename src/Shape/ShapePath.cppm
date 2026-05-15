module;

#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <QTransform>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iterator>
#include <cfloat>
#include <numbers>

module Shape.Path:Impl;

import Shape.Path;
import Shape.Types;
import Math;

namespace ArtifactCore {

// ========================================
// ShapePath::Impl 定義
// ========================================

class ShapePath::Impl {
public:
    QString name_;
    std::vector<PathCommand> commands_;
    mutable QRectF cachedBounds_;
    mutable bool dirty_ = true;

    Impl() = default;
    Impl(const Impl& other) = default;
    Impl& operator=(const Impl& other) = default;
    Impl(Impl&&) noexcept = default;

    void invalidate() const { dirty_ = true; }

    QRectF computeBounds() const {
        return toPainterPath().boundingRect();
    }

    // 現在のパスを QPainterPath に変換
    QPainterPath toPainterPath() const {
        QPainterPath path;
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
    impl_->commands_.push_back(PathCommand{PathCommandType::MoveTo, point});
    impl_->invalidate();
}

void ShapePath::moveTo(double x, double y) {
    moveTo(QPointF(x, y));
}

void ShapePath::lineTo(const QPointF& point) {
    impl_->commands_.push_back(PathCommand{PathCommandType::LineTo, point});
    impl_->invalidate();
}

void ShapePath::lineTo(double x, double y) {
    lineTo(QPointF(x, y));
}

void ShapePath::cubicTo(const QPointF& control1, const QPointF& control2, const QPointF& end) {
    impl_->commands_.push_back(PathCommand{PathCommandType::CubicTo, control1, control2, end});
    impl_->invalidate();
}

void ShapePath::cubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey) {
    cubicTo(QPointF(c1x, c1y), QPointF(c2x, c2y), QPointF(ex, ey));
}

void ShapePath::quadTo(const QPointF& control, const QPointF& end) {
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
    // 現在のパスをQPainterPathに変換
    QPainterPath current = toPainterPath();
    current.arcTo(rect, startAngle, sweepAngle);
    *this = fromPainterPath(current);
}

// ========================================
// 図形プリミティブ
// ========================================

void ShapePath::setRectangle(const QRectF& rect) {
    clear();
    moveTo(rect.topLeft());
    lineTo(rect.topRight());
    lineTo(rect.bottomRight());
    lineTo(rect.bottomLeft());
    close();
}

void ShapePath::setRectangle(double x, double y, double width, double height) {
    setRectangle(QRectF(x, y, width, height));
}

void ShapePath::setRoundedRect(const QRectF& rect, double radiusX, double radiusY) {
    QPainterPath path;
    path.addRoundedRect(rect, radiusX, radiusY);
    *this = fromPainterPath(path);
}

void ShapePath::setEllipse(const QRectF& rect) {
    clear();
    const double cx = rect.center().x();
    const double cy = rect.center().y();
    const double rx = rect.width() / 2.0;
    const double ry = rect.height() / 2.0;

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
    if (points.empty()) return;

    moveTo(points[0]);
    for (size_t i = 1; i < points.size(); ++i) {
        lineTo(points[i]);
    }
    if (closed) close();
}

void ShapePath::setStar(const QPointF& center, int points, double outerRadius, double innerRadius) {
    clear();
    if (points < 2) return;

    const double angleStep = std::numbers::pi / points;
    const double startAngle = -std::numbers::pi / 2;

    std::vector<QPointF> starPoints;
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

    setPolygon(starPoints, true);
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
        close();
    } else {
        if (!impl_->commands_.empty() && impl_->commands_.back().type == PathCommandType::Close) {
            impl_->commands_.pop_back();
            impl_->invalidate();
        }
    }
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
    QPainterPath path = toPainterPath();
    return path.contains(point);
}

QPointF ShapePath::pointAtPercent(double t) const {
    if (impl_->commands_.empty()) return QPointF();
    QPainterPath path = toPainterPath();
    if (path.isEmpty()) return QPointF();
    return path.pointAtPercent(std::clamp(t, 0.0, 1.0));
}

double ShapePath::length() const {
    QPainterPath path = toPainterPath();
    return path.length();
}

QPointF ShapePath::pointAtLength(double length) const {
    if (impl_->commands_.empty()) return QPointF();
    QPainterPath path = toPainterPath();
    if (path.isEmpty()) return QPointF();
    const double total = path.length();
    if (total <= 0.0) {
        return path.pointAtPercent(0.0);
    }
    return path.pointAtPercent(path.percentAtLength(std::clamp(length, 0.0, total)));
}

std::vector<BezierSegment> ShapePath::toSegments() const {
    std::vector<BezierSegment> segments;
    const auto& cmds = impl_->commands_;
    if (cmds.empty()) return segments;

    QPointF currentPos;  // 現在のパス位置
    bool hasCurrent = false;

    for (size_t i = 0; i < cmds.size(); ++i) {
        const auto& cmd = cmds[i];
        switch (cmd.type) {
            case PathCommandType::MoveTo:
                currentPos = cmd.points[0];
                hasCurrent = true;
                break;
            case PathCommandType::LineTo: {
                QPointF start = hasCurrent ? currentPos : QPointF(0, 0);
                QPointF end = cmd.points[0];
                segments.push_back(BezierSegment{start, start, end, end});
                currentPos = end;
                break;
            }
            case PathCommandType::CubicTo: {
                QPointF start = hasCurrent ? currentPos : QPointF(0, 0);
                segments.push_back(BezierSegment{start, cmd.points[0], cmd.points[1], cmd.points[2]});
                currentPos = cmd.points[2];
                break;
            }
            case PathCommandType::QuadTo: {
                QPointF start = hasCurrent ? currentPos : QPointF(0, 0);
                QPointF cp = cmd.points[0];
                QPointF end = cmd.points[1];
                QPointF cp1 = start + 2.0 / 3.0 * (cp - start);
                QPointF cp2 = end + 2.0 / 3.0 * (cp - end);
                segments.push_back(BezierSegment{start, cp1, cp2, end});
                currentPos = end;
                break;
            }
            case PathCommandType::Close:
                hasCurrent = false;
                break;
        }
    }
    return segments;
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
    std::vector<PathCommand> reversed;
    reversed.reserve(impl_->commands_.size());

    for (auto it = impl_->commands_.rbegin(); it != impl_->commands_.rend(); ++it) {
        const PathCommand& cmd = *it;
        switch (cmd.type) {
            case PathCommandType::MoveTo:
                reversed.push_back(cmd);
                break;
            case PathCommandType::LineTo:
                reversed.push_back(cmd);
                break;
            case PathCommandType::CubicTo: {
                PathCommand newCmd(PathCommandType::CubicTo, cmd.points[2], cmd.points[1], cmd.points[0]);
                reversed.push_back(newCmd);
                break;
            }
            case PathCommandType::QuadTo: {
                PathCommand newCmd(PathCommandType::QuadTo, cmd.points[1], cmd.points[0]);
                reversed.push_back(newCmd);
                break;
            }
            case PathCommandType::Close:
                reversed.push_back(cmd);
                break;
        }
    }
    impl_->commands_ = std::move(reversed);
    impl_->invalidate();
}

void ShapePath::addPath(const ShapePath& other) {
    if (other.isEmpty()) return;
    QPainterPath combined = toPainterPath();
    combined.addPath(other.toPainterPath());
    *this = fromPainterPath(combined);
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

} // namespace ArtifactCore
