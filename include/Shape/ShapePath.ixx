module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <vector>

export module Shape.Path;

import std;
import Shape.Types;

export namespace ArtifactCore {

/// ベジェパス（シェイプの基本パスデータ）
/// 
/// ベジェ曲線で構成されるパス。アニメーション対応。
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
    
    /// パスをクリア
    void clear();
    
    /// 新しいサブパスを開始
    void moveTo(const QPointF& point);
    void moveTo(double x, double y);
    
    /// 直線を追加
    void lineTo(const QPointF& point);
    void lineTo(double x, double y);
    
    /// 3次ベジェ曲線を追加
    void cubicTo(const QPointF& control1, const QPointF& control2, const QPointF& end);
    void cubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey);
    
    /// 2次ベジェ曲線を追加
    void quadTo(const QPointF& control, const QPointF& end);
    void quadTo(double cx, double cy, double ex, double ey);
    
    /// 現在のサブパスを閉じる
    void close();
    
    /// 円弧を追加
    void arcTo(const QRectF& rect, double startAngle, double sweepAngle);
    
    // ========================================
    // 図形プリミティブ
    // ========================================
    
    /// 矩形パスを設定
    void setRectangle(const QRectF& rect);
    void setRectangle(double x, double y, double width, double height);
    
    /// 角丸矩形を設定
    void setRoundedRect(const QRectF& rect, double radiusX, double radiusY);
    
    /// 楕円パスを設定
    void setEllipse(const QRectF& rect);
    void setEllipse(double cx, double cy, double rx, double ry);
    
    /// 多角形パスを設定
    void setPolygon(const std::vector<QPointF>& points, bool closed = true);
    
    /// 星形パスを設定
    void setStar(const QPointF& center, int points, double outerRadius, double innerRadius);
    
    // ========================================
    // プロパティ
    // ========================================
    
    /// パス名
    QString name() const;
    void setName(const QString& name);
    
    /// 閉じたパスかどうか
    bool isClosed() const;
    void setClosed(bool closed);
    
    /// 空かどうか
    bool isEmpty() const;
    
    /// コマンド数
    int commandCount() const;
    
    /// コマンド取得
    const std::vector<PathCommand>& commands() const;
    
    // ========================================
    // ジオメトリ
    // ========================================
    
    /// バウンディングボックス
    QRectF boundingRect() const;
    
    /// 点がパス内にあるか
    bool contains(const QPointF& point) const;
    
    /// 指定位置のパス上の点を取得
    QPointF pointAtPercent(double t) const;
    
    /// パスの長さ
    double length() const;
    
    /// パスに沿った点を取得
    QPointF pointAtLength(double length) const;
    
    /// セグメントに分割
    std::vector<BezierSegment> toSegments() const;
    
    // ========================================
    // 変換
    // ========================================
    
    /// 全体を移動
    void translate(const QPointF& offset);
    
    /// 全体をスケール
    void scale(const QPointF& center, double sx, double sy);
    
    /// 全体を回転
    void rotate(const QPointF& center, double angle);
    
    /// 変換行列を適用
    void transform(const QTransform& matrix);
    
    // ========================================
    // Qt連携
    // ========================================
    
    /// QPainterPathに変換
    QPainterPath toPainterPath() const;
    
    /// QPainterPathから構築
    static ShapePath fromPainterPath(const QPainterPath& path);
    
    // ========================================
    // ユーティリティ
    // ========================================
    
    /// パスのコピー
    ShapePath clone() const;
    
    /// パスを反転
    void reverse();
    
    /// 他のパスと結合
    void addPath(const ShapePath& other);
    
    /// 単純化
    void simplify();
    
private:
    class Impl;
    Impl* impl_;
};

/// 矩形シェイプ
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

/// 楕円シェイプ
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

/// 多角形シェイプ
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

/// 星形シェイプ
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

} // namespace ArtifactCore