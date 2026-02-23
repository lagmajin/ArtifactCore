module;

#include <QString>
#include <QRectF>
#include <vector>
#include <memory>

export module Shape.Group;

import std;
import Shape.Types;
import Shape.Path;

export namespace ArtifactCore {

/// 前方宣言
class ShapeGroup;

/// シェイプ要素の基底クラス
class ShapeElement {
public:
    virtual ~ShapeElement() = default;
    
    /// シェイプ種別
    virtual ShapeType type() const = 0;
    
    /// 名前
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }
    
    /// 表示
    bool isVisible() const { return visible_; }
    void setVisible(bool visible) { visible_ = visible; }
    
    /// ロック
    bool isLocked() const { return locked_; }
    void setLocked(bool locked) { locked_ = locked; }
    
    /// バウンディングボックス
    virtual QRectF boundingRect() const = 0;
    
    /// トランスフォーム
    virtual ShapeTransform& transform() = 0;
    virtual const ShapeTransform& transform() const = 0;
    
    /// 親グループ
    ShapeGroup* parent() const { return parent_; }
    
    /// クローン
    virtual std::unique_ptr<ShapeElement> clone() const = 0;

protected:
    QString name_;
    bool visible_ = true;
    bool locked_ = false;
    ShapeGroup* parent_ = nullptr;
    
    friend class ShapeGroup;
};

/// シェイプグループ（複数シェイプのコンテナ）
class ShapeGroup : public ShapeElement {
public:
    ShapeGroup();
    ~ShapeGroup() override;
    
    ShapeType type() const override { return ShapeType::Group; }
    
    // ========================================
    // 子要素管理
    // ========================================
    
    /// 子要素を追加
    void addChild(std::unique_ptr<ShapeElement> child);
    
    /// 子要素を挿入
    void insertChild(int index, std::unique_ptr<ShapeElement> child);
    
    /// 子要素を削除
    void removeChild(ShapeElement* child);
    void removeChild(int index);
    
    /// 全子要素を削除
    void clearChildren();
    
    /// 子要素数
    int childCount() const;
    
    /// 子要素取得
    ShapeElement* childAt(int index) const;
    
    /// 全子要素取得
    std::vector<ShapeElement*> children() const;
    
    /// インデックス取得
    int indexOf(ShapeElement* child) const;
    
    // ========================================
    // プロパティ
    // ========================================
    
    QRectF boundingRect() const override;
    
    ShapeTransform& transform() override { return transform_; }
    const ShapeTransform& transform() const override { return transform_; }
    
    // ========================================
    // 操作
    // ========================================
    
    /// 全体を移動
    void translate(const QPointF& offset);
    
    /// 全体をスケール
    void scale(const QPointF& center, double sx, double sy);
    
    /// 全体を回転
    void rotate(const QPointF& center, double angle);
    
    /// クローン
    std::unique_ptr<ShapeElement> clone() const override;
    
private:
    std::vector<std::unique_ptr<ShapeElement>> children_;
    ShapeTransform transform_;
};

/// パスシェイプ（実際の描画要素）
class PathShape : public ShapeElement {
public:
    PathShape();
    explicit PathShape(const ShapePath& path);
    ~PathShape() override;
    
    ShapeType type() const override { return ShapeType::Path; }
    
    // ========================================
    // パス
    // ========================================
    
    ShapePath& path();
    const ShapePath& path() const;
    void setPath(const ShapePath& path);
    
    // ========================================
    // ストローク・フィル
    // ========================================
    
    StrokeSettings& stroke();
    const StrokeSettings& stroke() const;
    void setStroke(const StrokeSettings& stroke);
    
    FillSettings& fill();
    const FillSettings& fill() const;
    void setFill(const FillSettings& fill);
    
    // ========================================
    // プロパティ
    // ========================================
    
    QRectF boundingRect() const override;
    
    ShapeTransform& transform() override { return transform_; }
    const ShapeTransform& transform() const override { return transform_; }
    
    // ========================================
    // 描画
    // ========================================
    
    /// QPainterPathに変換（トランスフォーム適用済み）
    QPainterPath toPainterPath() const;
    
    /// クローン
    std::unique_ptr<ShapeElement> clone() const override;
    
private:
    class Impl;
    Impl* impl_;
    ShapeTransform transform_;
};

/// 矩形シェイプ
class RectanglePathShape : public PathShape {
public:
    RectanglePathShape();
    explicit RectanglePathShape(const QRectF& rect);
    
    ShapeType type() const override { return ShapeType::Rectangle; }
    
    QRectF rect() const;
    void setRect(const QRectF& rect);
    
    double cornerRadius() const;
    void setCornerRadius(double radius);
    
    std::unique_ptr<ShapeElement> clone() const override;
    
private:
    void updatePath();
    
    QRectF rect_;
    double cornerRadius_ = 0.0;
};

/// 楕円シェイプ
class EllipsePathShape : public PathShape {
public:
    EllipsePathShape();
    explicit EllipsePathShape(const QRectF& rect);
    
    ShapeType type() const override { return ShapeType::Ellipse; }
    
    QRectF rect() const;
    void setRect(const QRectF& rect);
    
    std::unique_ptr<ShapeElement> clone() const override;
    
private:
    void updatePath();
    
    QRectF rect_;
};

/// 多角形シェイプ
class PolygonPathShape : public PathShape {
public:
    PolygonPathShape();
    explicit PolygonPathShape(const std::vector<QPointF>& points);
    
    ShapeType type() const override { return ShapeType::Polygon; }
    
    std::vector<QPointF> points() const;
    void setPoints(const std::vector<QPointF>& points);
    
    int pointCount() const;
    
    bool isClosed() const;
    void setClosed(bool closed);
    
    std::unique_ptr<ShapeElement> clone() const override;
    
private:
    void updatePath();
    
    std::vector<QPointF> points_;
    bool closed_ = true;
};

/// 星形シェイプ
class StarPathShape : public PathShape {
public:
    StarPathShape();
    StarPathShape(const QPointF& center, int points, double outerRadius, double innerRadius);
    
    ShapeType type() const override { return ShapeType::Star; }
    
    QPointF center() const;
    void setCenter(const QPointF& center);
    
    int points() const;
    void setPoints(int points);
    
    double outerRadius() const;
    void setOuterRadius(double radius);
    
    double innerRadius() const;
    void setInnerRadius(double radius);
    
    std::unique_ptr<ShapeElement> clone() const override;
    
private:
    void updatePath();
    
    QPointF center_;
    int points_ = 5;
    double outerRadius_ = 100.0;
    double innerRadius_ = 50.0;
};

} // namespace ArtifactCore