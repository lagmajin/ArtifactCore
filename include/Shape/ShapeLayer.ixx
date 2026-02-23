module;

#include <QString>
#include <QRectF>
#include <QImage>
#include <memory>

export module Shape.Layer;

import std;
import Shape.Types;
import Shape.Path;
import Shape.Group;

export namespace ArtifactCore {

/// シェイプレイヤー
/// 
/// ベクターシェイプを含むレイヤー。
/// After Effectsのシェイプレイヤーに相当。
class ShapeLayer {
public:
    ShapeLayer();
    ~ShapeLayer();
    
    ShapeLayer(const ShapeLayer& other);
    ShapeLayer& operator=(const ShapeLayer& other);
    ShapeLayer(ShapeLayer&& other) noexcept;
    ShapeLayer& operator=(ShapeLayer&& other) noexcept;
    
    // ========================================
    // レイヤープロパティ
    // ========================================
    
    /// レイヤー名
    QString name() const;
    void setName(const QString& name);
    
    /// レイヤーID
    int layerId() const;
    
    /// 表示
    bool isVisible() const;
    void setVisible(bool visible);
    
    /// ロック
    bool isLocked() const;
    void setLocked(bool locked);
    
    /// ソロ
    bool isSolo() const;
    void setSolo(bool solo);
    
    // ========================================
    // コンテンツ
    // ========================================
    
    /// ルートグループ
    ShapeGroup* content();
    const ShapeGroup* content() const;
    
    /// シェイプを追加
    void addShape(std::unique_ptr<ShapeElement> shape);
    
    /// 全シェイプをクリア
    void clearContent();
    
    /// シェイプ数
    int shapeCount() const;
    
    // ========================================
    // トランスフォーム
    // ========================================
    
    /// レイヤートランスフォーム
    ShapeTransform& transform();
    const ShapeTransform& transform() const;
    
    /// アンカーポイント
    Point2DValue anchorPoint() const;
    void setAnchorPoint(const Point2DValue& anchor);
    
    /// 位置
    Point2DValue position() const;
    void setPosition(const Point2DValue& position);
    
    /// スケール
    Point2DValue scale() const;
    void setScale(const Point2DValue& scale);
    
    /// 回転
    double rotation() const;
    void setRotation(double angle);
    
    /// 不透明度
    double opacity() const;
    void setOpacity(double opacity);
    
    // ========================================
    // ジオメトリ
    // ========================================
    
    /// バウンディングボックス
    QRectF boundingRect() const;
    
    /// 点が含まれるか
    bool contains(const QPointF& point) const;
    
    // ========================================
    // 描画・レンダリング
    // ========================================
    
    /// 指定サイズでレンダリング
    QImage render(const QSize& size, double time = 0.0) const;
    
    /// QPainterPathとして取得
    QPainterPath toPainterPath() const;
    
    // ========================================
    // ブレンドモード
    // ========================================
    
    /// ブレンドモード
    enum class BlendMode {
        Normal,
        Multiply,
        Screen,
        Overlay,
        Darken,
        Lighten,
        ColorDodge,
        ColorBurn,
        HardLight,
        SoftLight,
        Difference,
        Exclusion,
        Hue,
        Saturation,
        Color,
        Luminosity
    };
    
    BlendMode blendMode() const;
    void setBlendMode(BlendMode mode);
    
    // ========================================
    // ユーティリティ
    // ========================================
    
    /// 複製
    ShapeLayer clone() const;
    
    /// SVGとしてエクスポート
    QString toSvg() const;
    
    /// SVGからインポート
    static ShapeLayer fromSvg(const QString& svgContent);
    
    // ========================================
    // プリミティブ作成
    // ========================================
    
    /// 空のシェイプレイヤー作成
    static ShapeLayer createEmpty();
    
    /// 矩形シェイプレイヤー作成
    static ShapeLayer createRectangle(const QRectF& rect, 
                                       const FillSettings& fill = FillSettings(),
                                       const StrokeSettings& stroke = StrokeSettings());
    
    /// 楕円シェイプレイヤー作成
    static ShapeLayer createEllipse(const QRectF& rect,
                                     const FillSettings& fill = FillSettings(),
                                     const StrokeSettings& stroke = StrokeSettings());
    
    /// 星形シェイプレイヤー作成
    static ShapeLayer createStar(const QPointF& center, int points,
                                  double outerRadius, double innerRadius,
                                  const FillSettings& fill = FillSettings(),
                                  const StrokeSettings& stroke = StrokeSettings());
    
    /// 多角形シェイプレイヤー作成
    static ShapeLayer createPolygon(const std::vector<QPointF>& points,
                                     const FillSettings& fill = FillSettings(),
                                     const StrokeSettings& stroke = StrokeSettings());

private:
    class Impl;
    Impl* impl_;
};

/// シェイプレイヤーファクトリ
class ShapeLayerFactory {
public:
    /// パスからシェイプレイヤー作成
    static ShapeLayer fromPath(const ShapePath& path,
                                const FillSettings& fill,
                                const StrokeSettings& stroke);
    
    /// QPainterPathからシェイプレイヤー作成
    static ShapeLayer fromPainterPath(const QPainterPath& path,
                                       const FillSettings& fill,
                                       const StrokeSettings& stroke);
    
    /// テキストからシェイプレイヤー作成（パス化）
    static ShapeLayer fromText(const QString& text,
                                const QString& fontFamily,
                                int fontSize,
                                const QPointF& position,
                                const FillSettings& fill);
};

} // namespace ArtifactCore