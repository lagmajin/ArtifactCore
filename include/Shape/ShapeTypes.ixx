module;

#include <QString>
#include <QPointF>
#include <QColor>
#include <QRectF>
#include <vector>

export module Shape.Types;

import std;
import Property.Types;

export namespace ArtifactCore {

/// シェイプ種別
enum class ShapeType {
    Path,       ///< 自由パス
    Rectangle,  ///< 矩形
    Ellipse,    ///< 楕円
    Polygon,    ///< 多角形
    Star,       ///< 星形
    Line,       ///< 直線
    Group       ///< グループ
};

/// 線端形状
enum class LineCap {
    Butt,       ///< 平ら
    Round,      ///< 丸
    Square      ///< 四角
};

/// 線結合形状
enum class LineJoin {
    Miter,      ///< 角張った結合
    Round,      ///< 丸い結合
    Bevel       ///< 斜め結合
};

/// ベジェセグメント
struct BezierSegment {
    QPointF p0;      ///< 始点
    QPointF cp1;     ///< 制御点1
    QPointF cp2;     ///< 制御点2
    QPointF p1;      ///< 終点
    
    BezierSegment() = default;
    BezierSegment(const QPointF& start, const QPointF& control1, 
                  const QPointF& control2, const QPointF& end)
        : p0(start), cp1(control1), cp2(control2), p1(end) {}
    
    /// t(0-1)での点を取得
    QPointF pointAt(double t) const {
        double u = 1.0 - t;
        double tt = t * t;
        double uu = u * u;
        double uuu = uu * u;
        double ttt = tt * t;
        
        return uuu * p0 + 3.0 * uu * t * cp1 + 3.0 * u * tt * cp2 + ttt * p1;
    }
    
    /// バウンディングボックス
    QRectF boundingRect() const {
        double minX = std::min({p0.x(), cp1.x(), cp2.x(), p1.x()});
        double maxX = std::max({p0.x(), cp1.x(), cp2.x(), p1.x()});
        double minY = std::min({p0.y(), cp1.y(), cp2.y(), p1.y()});
        double maxY = std::max({p0.y(), cp1.y(), cp2.y(), p1.y()});
        return QRectF(minX, minY, maxX - minX, maxY - minY);
    }
};

/// パスコマンド種別
enum class PathCommandType {
    MoveTo,     ///< ペン移動
    LineTo,     ///< 直線
    CubicTo,    ///< 3次ベジェ曲線
    QuadTo,     ///< 2次ベジェ曲線
    Close       ///< パス閉じる
};

/// パスコマンド
struct PathCommand {
    PathCommandType type;
    QPointF points[3];  ///< 最大3点（MoveToは1点、CubicToは3点）
    
    explicit PathCommand(PathCommandType t) : type(t) {}
    PathCommand(PathCommandType t, const QPointF& p1) : type(t) {
        points[0] = p1;
    }
    PathCommand(PathCommandType t, const QPointF& p1, const QPointF& p2) : type(t) {
        points[0] = p1; points[1] = p2;
    }
    PathCommand(PathCommandType t, const QPointF& p1, const QPointF& p2, const QPointF& p3) : type(t) {
        points[0] = p1; points[1] = p2; points[2] = p3;
    }
};

/// ストローク設定
struct StrokeSettings {
    bool enabled = true;
    QColor color = Qt::white;
    double width = 1.0;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    double miterLimit = 4.0;
    double dashOffset = 0.0;
    std::vector<double> dashPattern;  ///< 破線パターン
    
    StrokeSettings() = default;
    StrokeSettings(const QColor& c, double w) 
        : enabled(true), color(c), width(w) {}
    
    bool isDashed() const { return !dashPattern.empty(); }
};

/// フィル設定
struct FillSettings {
    bool enabled = true;
    QColor color = Qt::white;
    
    /// グラデーション対応（将来拡張）
    enum class FillType {
        Solid,          ///< 単色
        Linear,         ///< 線形グラデーション
        Radial,         ///< 円形グラデーション
        Conic           ///< 扇形グラデーション
    };
    FillType type = FillType::Solid;
    
    FillSettings() = default;
    explicit FillSettings(const QColor& c) : enabled(true), color(c) {}
};

/// トランスフォーム設定
struct ShapeTransform {
    Point2DValue anchor;    ///< アンカーポイント
    Point2DValue position;  ///< 位置
    Point2DValue scale;     ///< スケール (1,1が100%)
    double rotation = 0.0;  ///< 回転角度（度）
    double opacity = 1.0;   ///< 不透明度
    
    ShapeTransform() {
        scale = Point2DValue(1.0, 1.0);
    }
    
    /// バウンディングボックスに合わせてトランスフォームをリセット
    void resetTo(const QRectF& bounds) {
        anchor = Point2DValue(bounds.center().x(), bounds.center().y());
        position = anchor;
        scale = Point2DValue(1.0, 1.0);
        rotation = 0.0;
    }
};

} // namespace ArtifactCore