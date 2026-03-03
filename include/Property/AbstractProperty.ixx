module;

#include <QString>
#include <QVariant>
#include <QColor>
#include <vector>
#include "../Define/DllExportMacro.hpp"
export module Property.Abstract;

import std;
import Color.Float;
import Utils.String.UniString;
import Time.Rational;

import Math.Interpolate;


export namespace ArtifactCore {

enum class PropertyType {
    Float,
    Integer,
    Boolean,
    Color,
    String
};

struct KeyFrame {
    RationalTime time;
    QVariant value;
};

class LIBRARY_DLL_API AbstractProperty {
private:
    class Impl;
    Impl* pImpl;

public:
    AbstractProperty();
    ~AbstractProperty();

    // Copy/Move - declarations only (impl in .cppm)
    AbstractProperty(const AbstractProperty& other);
    AbstractProperty& operator=(const AbstractProperty& other);
    AbstractProperty(AbstractProperty&& other) noexcept;
    AbstractProperty& operator=(AbstractProperty&& other) noexcept;

    // Getters
    QString getName() const;
    PropertyType getType() const;
    QVariant getValue() const;
    QVariant getDefaultValue() const;
    QVariant getMinValue() const;
    QVariant getMaxValue() const;
    bool isAnimatable() const;
    QColor getColorValue() const;

    // Setters
    void setName(const QString& name);
    void setType(PropertyType type);
    void setValue(const QVariant& value);
    void setDefaultValue(const QVariant& value);
    void setMinValue(const QVariant& value);
    void setMaxValue(const QVariant& value);
    void setAnimatable(bool animatable);
    void setColorValue(const QColor& color);

    /// @brief 表示優先度を設定する。
    /// 値が小さいほど高優先度（先頭に近い位置に表示される）。
    /// デフォルトは 0。
    /// @param priority 表示優先度（負の値も可）
    void setDisplayPriority(int priority);

    /// @brief 表示優先度を返す。
    int displayPriority() const;

    // KeyFrame operations
    void addKeyFrame(const RationalTime& time, const QVariant& value);
    void removeKeyFrame(const RationalTime& time);
    void clearKeyFrames();
    std::vector<KeyFrame> getKeyFrames() const;
    bool hasKeyFrameAt(const RationalTime& time) const;
    QVariant interpolateValue(const RationalTime& time) const;

    // Validation
    void clampValue();
    bool isValueInRange(const QVariant& value) const;
};

};