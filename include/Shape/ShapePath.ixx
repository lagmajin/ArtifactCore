module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <QTransform>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

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
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Shape.Path;

import Shape.Types;

export namespace ArtifactCore {

class ShapePath {
public:
    ShapePath();
    ~ShapePath();

    ShapePath(const ShapePath& other);
    ShapePath& operator=(const ShapePath& other);
    ShapePath(ShapePath&& other) noexcept;
    ShapePath& operator=(ShapePath&& other) noexcept;

    // ========================================
    // パス構築
    // ========================================

    void clear();
    void moveTo(const QPointF& point);
    void moveTo(double x, double y);
    void lineTo(const QPointF& point);
    void lineTo(double x, double y);
    void cubicTo(const QPointF& control1, const QPointF& control2, const QPointF& end);
    void cubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey);
    void quadTo(const QPointF& control, const QPointF& end);
    void quadTo(double cx, double cy, double ex, double ey);
    void close();
    void arcTo(const QRectF& rect, double startAngle, double sweepAngle);

    // ========================================
    // 図形プリミティブ
    // ========================================

    void setRectangle(const QRectF& rect);
    void setRectangle(double x, double y, double width, double height);
    void setRoundedRect(const QRectF& rect, double radiusX, double radiusY);
    void setEllipse(const QRectF& rect);
    void setEllipse(double cx, double cy, double rx, double ry);
    void setPolygon(const std::vector<QPointF>& points, bool closed = true);
    void setStar(const QPointF& center, int points, double outerRadius, double innerRadius);

    // ========================================
    // プロパティ
    // ========================================

    QString name() const;
    void setName(const QString& name);
    bool isClosed() const;
    void setClosed(bool closed);
    double opacity() const;
    void setOpacity(double opacity);
    bool isEmpty() const;
    int commandCount() const;
    const std::vector<PathCommand>& commands() const;

    // ========================================
    // ジオメトリ
    // ========================================

    QRectF boundingRect() const;
    bool contains(const QPointF& point) const;
    QPointF pointAtPercent(double t) const;
    double length() const;
    QPointF pointAtLength(double length) const;
    std::vector<BezierSegment> toSegments() const;

    /// パラメータ t における接線ベクトル（正規化済み）
    QPointF tangentAtPercent(double t) const;

    /// パラメータ t における法線ベクトル（右方向、正規化済み）
    QPointF normalAtPercent(double t) const;

    /// 時計回りか（サブパスごとに判定し最初の閉じたサブパスで決定）
    bool isClockwise() const;

    /// パス全体をオフセット（膨張・収縮）。delta > 0 で膨張。
    ShapePath offsetPath(double delta, int subdivisions = 16) const;

    // ========================================
    // 変換
    // ========================================

    void translate(const QPointF& offset);
    void scale(const QPointF& center, double sx, double sy);
    void rotate(const QPointF& center, double angle);
    void transform(const QTransform& matrix);

    // ========================================
    // Qt連携
    // ========================================

    QPainterPath toPainterPath() const;
    static ShapePath fromPainterPath(const QPainterPath& path);

    // ========================================
    // シリアライズ
    // ========================================

    QJsonObject toJson() const;
    static ShapePath fromJson(const QJsonObject& obj);

    // ========================================
    // ユーティリティ
    // ========================================

    ShapePath clone() const;
    void reverse();
    void addPath(const ShapePath& other);
    void simplify();

    std::vector<ShapePath> subpaths() const;
    std::vector<QPointF> sampleEquidistant(int count) const;
    static ShapePath interpolate(const ShapePath& from, const ShapePath& to, double t, int sampleCount = 64);
    double area() const;
    QPointF centroid() const;

private:
    class Impl;
    Impl* impl_;

    QPointF getStartPoint(const PathCommand& cmd) const;
    QPointF getEndPoint(const PathCommand& cmd) const;
    double cubicApproxLength(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3) const;
    double quadApproxLength(const QPointF& p0, const QPointF& p1, const QPointF& p2) const;
    QPointF cubicPointAtLength(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, double targetLen) const;
    QPointF quadPointAtLength(const QPointF& p0, const QPointF& p1, const QPointF& p2, double targetLen) const;
};

class RectangleShape {
public:
    RectangleShape();
    explicit RectangleShape(const QRectF& rect);
    RectangleShape(double x, double y, double width, double height);

    QRectF rect() const;
    void setRect(const QRectF& rect);
    double cornerRadius() const;
    void setCornerRadius(double radius);
    ShapePath toPath() const;

private:
    QRectF rect_;
    double cornerRadius_ = 0.0;
};

class EllipseShape {
public:
    EllipseShape();
    explicit EllipseShape(const QRectF& rect);
    EllipseShape(double cx, double cy, double rx, double ry);

    QRectF rect() const;
    void setRect(const QRectF& rect);
    QPointF center() const;
    double radiusX() const;
    double radiusY() const;
    ShapePath toPath() const;

private:
    QRectF rect_;
};

class PolygonShape {
public:
    PolygonShape();
    explicit PolygonShape(const std::vector<QPointF>& points);

    void setPoints(const std::vector<QPointF>& points);
    const std::vector<QPointF>& points() const;
    int pointCount() const;
    QPointF pointAt(int index) const;
    void setPointAt(int index, const QPointF& point);
    bool isClosed() const;
    void setClosed(bool closed);
    ShapePath toPath() const;

private:
    std::vector<QPointF> points_;
    bool closed_ = true;
};

class StarShape {
public:
    StarShape();
    StarShape(const QPointF& center, int points, double outerRadius, double innerRadius);

    QPointF center() const;
    void setCenter(const QPointF& center);
    int points() const;
    void setPoints(int points);
    double outerRadius() const;
    void setOuterRadius(double radius);
    double innerRadius() const;
    void setInnerRadius(double radius);
    double rotation() const;
    void setRotation(double angle);
    ShapePath toPath() const;

private:
    QPointF center_;
    int points_ = 5;
    double outerRadius_ = 100.0;
    double innerRadius_ = 50.0;
    double rotation_ = 0.0;
};

}
