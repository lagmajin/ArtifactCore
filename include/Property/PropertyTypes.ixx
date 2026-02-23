module;

#include <QPointF>
#include <QVector3D>
#include <QVariant>
#include <QColor>

export module Property.Types;

import std;

export namespace ArtifactCore {

/// プロパティ型の拡張（AbstractPropertyのPropertyTypeを拡張）
enum class ExtendedPropertyType {
    // 基本型
    Float,
    Integer,
    Boolean,
    Color,
    String,
    
    // 拡張型
    Point2D,       ///< QPointF - 2D座標
    Point3D,       ///< QVector3D - 3D座標
    Size2D,        ///< QSizeF - 2Dサイズ
    Rect,          ///< QRectF - 矩形
    Vector2D,      ///< 2次元ベクトル（方向付き）
    Vector3D,      ///< 3次元ベクトル（方向付き）
    Angle,         ///< 角度（度数法）
    Percentage,    ///< パーセンテージ (0-100)
    FloatRange     ///< 範囲付きfloat (min-max)
};

/// 2Dポイントプロパティ
struct Point2DValue {
    double x = 0.0;
    double y = 0.0;
    
    Point2DValue() = default;
    Point2DValue(double x_, double y_) : x(x_), y(y_) {}
    Point2DValue(const QPointF& p) : x(p.x()), y(p.y()) {}
    
    QPointF toQPointF() const { return QPointF(x, y); }
    
    bool operator==(const Point2DValue& other) const {
        return qFuzzyCompare(x, other.x) && qFuzzyCompare(y, other.y);
    }
    
    bool operator!=(const Point2DValue& other) const {
        return !(*this == other);
    }
    
    Point2DValue operator+(const Point2DValue& other) const {
        return Point2DValue(x + other.x, y + other.y);
    }
    
    Point2DValue operator-(const Point2DValue& other) const {
        return Point2DValue(x - other.x, y - other.y);
    }
    
    Point2DValue operator*(double scalar) const {
        return Point2DValue(x * scalar, y * scalar);
    }
    
    double length() const {
        return std::sqrt(x * x + y * y);
    }
    
    double lengthSquared() const {
        return x * x + y * y;
    }
    
    Point2DValue normalized() const {
        double len = length();
        if (len > 0) {
            return Point2DValue(x / len, y / len);
        }
        return *this;
    }
    
    static double distance(const Point2DValue& a, const Point2DValue& b) {
        return (b - a).length();
    }
};

/// 3Dポイントプロパティ
struct Point3DValue {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    
    Point3DValue() = default;
    Point3DValue(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Point3DValue(const QVector3D& v) : x(v.x()), y(v.y()), z(v.z()) {}
    
    QVector3D toQVector3D() const { return QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }
    
    bool operator==(const Point3DValue& other) const {
        return qFuzzyCompare(x, other.x) && qFuzzyCompare(y, other.y) && qFuzzyCompare(z, other.z);
    }
    
    Point3DValue operator+(const Point3DValue& other) const {
        return Point3DValue(x + other.x, y + other.y, z + other.z);
    }
    
    Point3DValue operator-(const Point3DValue& other) const {
        return Point3DValue(x - other.x, y - other.y, z - other.z);
    }
    
    Point3DValue operator*(double scalar) const {
        return Point3DValue(x * scalar, y * scalar, z * scalar);
    }
    
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    Point3DValue normalized() const {
        double len = length();
        if (len > 0) {
            return Point3DValue(x / len, y / len, z / len);
        }
        return *this;
    }
};

/// 2Dサイズプロパティ
struct Size2DValue {
    double width = 0.0;
    double height = 0.0;
    
    Size2DValue() = default;
    Size2DValue(double w, double h) : width(w), height(h) {}
    
    bool isEmpty() const { return width <= 0 || height <= 0; }
    double area() const { return width * height; }
    double aspectRatio() const { return height > 0 ? width / height : 0; }
};

/// 矩形プロパティ
struct RectValue {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    
    RectValue() = default;
    RectValue(double x_, double y_, double w, double h) : x(x_), y(y_), width(w), height(h) {}
    RectValue(const QRectF& r) : x(r.x()), y(r.y()), width(r.width()), height(r.height()) {}
    
    QRectF toQRectF() const { return QRectF(x, y, width, height); }
    
    bool contains(const Point2DValue& p) const {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }
    
    Point2DValue center() const {
        return Point2DValue(x + width / 2.0, y + height / 2.0);
    }
    
    bool isEmpty() const { return width <= 0 || height <= 0; }
    double area() const { return width * height; }
};

/// 補間関数（QPointF用）
inline Point2DValue interpolatePoint2D(const Point2DValue& a, const Point2DValue& b, double t) {
    return Point2DValue(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    );
}

/// 補間関数（QVector3D用）
inline Point3DValue interpolatePoint3D(const Point3DValue& a, const Point3DValue& b, double t) {
    return Point3DValue(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    );
}

/// QVariant変換ヘルパー
namespace PropertyVariantHelper {
    inline QVariant toVariant(const Point2DValue& p) {
        return QVariant::fromValue(QPointF(p.x, p.y));
    }
    
    inline Point2DValue toPoint2D(const QVariant& v) {
        if (v.canConvert<QPointF>()) {
            return Point2DValue(v.toPointF());
        }
        return Point2DValue();
    }
    
    inline QVariant toVariant(const Point3DValue& p) {
        return QVariant::fromValue(QVector3D(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)));
    }
    
    inline Point3DValue toPoint3D(const QVariant& v) {
        if (v.canConvert<QVector3D>()) {
            return Point3DValue(v.value<QVector3D>());
        }
        return Point3DValue();
    }
    
    inline QVariant toVariant(const Size2DValue& s) {
        QStringList list;
        list << QString::number(s.width) << QString::number(s.height);
        return QVariant(list);
    }
    
    inline QVariant toVariant(const RectValue& r) {
        QStringList list;
        list << QString::number(r.x) << QString::number(r.y) 
             << QString::number(r.width) << QString::number(r.height);
        return QVariant(list);
    }
}

} // namespace ArtifactCore

// QMetaType登録（Qt型システムとの統合）
Q_DECLARE_METATYPE(ArtifactCore::Point2DValue)
Q_DECLARE_METATYPE(ArtifactCore::Point3DValue)
Q_DECLARE_METATYPE(ArtifactCore::Size2DValue)
Q_DECLARE_METATYPE(ArtifactCore::RectValue)