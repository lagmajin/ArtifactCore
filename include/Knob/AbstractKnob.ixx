module;

#include <QString>
#include <QVariant>
#include <QColor>
#include <vector>

export module Knob.Abstract;

import std;
import Time.Rational;

export namespace ArtifactCore {

enum class KnobType {
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

class AbstractKnob {
private:
    class Impl;
    Impl* pImpl;

public:
    AbstractKnob();
    ~AbstractKnob();

    // Getters
    QString getName() const;
    KnobType getType() const;
    QVariant getValue() const;
    QVariant getDefaultValue() const;
    QVariant getMinValue() const;
    QVariant getMaxValue() const;
    bool isAnimatable() const;
    QColor getColorValue() const;

    // Setters
    void setName(const QString& name);
    void setType(KnobType type);
    void setValue(const QVariant& value);
    void setDefaultValue(const QVariant& value);
    void setMinValue(const QVariant& value);
    void setMaxValue(const QVariant& value);
    void setAnimatable(bool animatable);
    void setColorValue(const QColor& color);

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