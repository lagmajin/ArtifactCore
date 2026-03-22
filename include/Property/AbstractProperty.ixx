module;

#include <QString>
#include <QVariant>
#include <QColor>
#include <vector>
#include "../Define/DllExportMacro.hpp"
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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Property.Abstract;




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
    String,
    ObjectReference  // ID 参照
};

struct KeyFrame {
    RationalTime time;
    QVariant value;
};

struct PropertyMetadata {
    QString displayLabel;
    QString unit;
    QString tooltip;
    QVariant hardMin;
    QVariant hardMax;
    QVariant softMin;
    QVariant softMax;
    QVariant step;
    
    // 参照タイプ用
    QString referenceTypeName;  // 参照可能タイプ名（例："LayerID", "CompositionID"）
    bool allowNull = true;      // null 許可
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
    PropertyMetadata metadata() const;

    // Setters
    void setName(const QString& name);
    void setType(PropertyType type);
    void setValue(const QVariant& value);
    void setDefaultValue(const QVariant& value);
    void setMinValue(const QVariant& value);
    void setMaxValue(const QVariant& value);
    void setAnimatable(bool animatable);
    void setColorValue(const QColor& color);
    void setMetadata(const PropertyMetadata& metadata);
    void setDisplayLabel(const QString& label);
    void setUnit(const QString& unit);
    void setTooltip(const QString& tooltip);
    void setStep(const QVariant& step);
    void setHardRange(const QVariant& minValue, const QVariant& maxValue);
    void setSoftRange(const QVariant& minValue, const QVariant& maxValue);

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
