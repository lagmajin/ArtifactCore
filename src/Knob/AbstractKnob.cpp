module;

#include <algorithm>
#include <cmath>
#include <QVariant>

module Knob.Abstract;

import Time.Rational;

namespace ArtifactCore {

class AbstractKnob::Impl {
public:
    QString m_name;
    KnobType m_type;
    QVariant m_value;
    QVariant m_defaultValue;
    QVariant m_minValue;
    QVariant m_maxValue;
    bool m_isAnimatable;
    std::vector<KeyFrame> m_keyFrames;
    QColor m_colorValue;

    Impl()
        : m_name("Knob")
        , m_type(KnobType::Float)
        , m_value(0.0)
        , m_defaultValue(0.0)
        , m_minValue(0.0)
        , m_maxValue(100.0)
        , m_isAnimatable(true)
        , m_colorValue(Qt::white)
    {
    }
};

AbstractKnob::AbstractKnob()
    : pImpl(new Impl())
{
}

AbstractKnob::~AbstractKnob()
{
    delete pImpl;
}

QString AbstractKnob::getName() const
{
    return pImpl->m_name;
}

KnobType AbstractKnob::getType() const
{
    return pImpl->m_type;
}

QVariant AbstractKnob::getValue() const
{
    return pImpl->m_value;
}

QVariant AbstractKnob::getDefaultValue() const
{
    return pImpl->m_defaultValue;
}

QVariant AbstractKnob::getMinValue() const
{
    return pImpl->m_minValue;
}

QVariant AbstractKnob::getMaxValue() const
{
    return pImpl->m_maxValue;
}

bool AbstractKnob::isAnimatable() const
{
    return pImpl->m_isAnimatable;
}

QColor AbstractKnob::getColorValue() const
{
    return pImpl->m_colorValue;
}

void AbstractKnob::setName(const QString& name)
{
    if (pImpl->m_name != name) {
        pImpl->m_name = name;
    }
}

void AbstractKnob::setType(KnobType type)
{
    if (pImpl->m_type != type) {
        pImpl->m_type = type;
        pImpl->m_keyFrames.clear();
    }
}

void AbstractKnob::setValue(const QVariant& value)
{
    if (pImpl->m_value != value) {
        pImpl->m_value = value;
        clampValue();
    }
}

void AbstractKnob::setDefaultValue(const QVariant& value)
{
    pImpl->m_defaultValue = value;
}

void AbstractKnob::setMinValue(const QVariant& value)
{
    pImpl->m_minValue = value;
    clampValue();
}

void AbstractKnob::setMaxValue(const QVariant& value)
{
    pImpl->m_maxValue = value;
    clampValue();
}

void AbstractKnob::setAnimatable(bool animatable)
{
    pImpl->m_isAnimatable = animatable;
    if (!animatable) {
        pImpl->m_keyFrames.clear();
    }
}

void AbstractKnob::setColorValue(const QColor& color)
{
    if (pImpl->m_colorValue != color) {
        pImpl->m_colorValue = color;
        pImpl->m_value = color;
    }
}

void AbstractKnob::addKeyFrame(const RationalTime& time, const QVariant& value)
{
    if (!pImpl->m_isAnimatable) return;

    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
                          [&time](const KeyFrame& kf) { return kf.time == time; });

    if (it != pImpl->m_keyFrames.end()) {
        it->value = value;
    } else {
        pImpl->m_keyFrames.push_back({time, value});
        std::sort(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
                 [](const KeyFrame& a, const KeyFrame& b) { return a.time < b.time; });
    }
}

void AbstractKnob::removeKeyFrame(const RationalTime& time)
{
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
                          [&time](const KeyFrame& kf) { return kf.time == time; });

    if (it != pImpl->m_keyFrames.end()) {
        pImpl->m_keyFrames.erase(it);
    }
}

void AbstractKnob::clearKeyFrames()
{
    pImpl->m_keyFrames.clear();
}

std::vector<KeyFrame> AbstractKnob::getKeyFrames() const
{
    return pImpl->m_keyFrames;
}

bool AbstractKnob::hasKeyFrameAt(const RationalTime& time) const
{
    return std::any_of(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
                      [&time](const KeyFrame& kf) { return kf.time == time; });
}

QVariant AbstractKnob::interpolateValue(const RationalTime& time) const
{
    if (pImpl->m_keyFrames.empty()) {
        return pImpl->m_value;
    }

    if (time < pImpl->m_keyFrames.front().time) {
        return pImpl->m_keyFrames.front().value;
    }

    if (time > pImpl->m_keyFrames.back().time) {
        return pImpl->m_keyFrames.back().value;
    }

    auto it = std::lower_bound(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(), time,
                              [](const KeyFrame& kf, const RationalTime& t) { return kf.time < t; });

    if (it == pImpl->m_keyFrames.begin()) {
        return it->value;
    }

    auto prev = std::prev(it);
    const KeyFrame& kf1 = *prev;
    const KeyFrame& kf2 = *it;

    // 補間パラメータ t を計算（0.0 ～ 1.0）
    double duration = (kf2.time - kf1.time).toSeconds();
    if (duration == 0.0) {
        return kf2.value;
    }

    double elapsed = (time - kf1.time).toSeconds();
    double t = elapsed / duration;
    t = std::clamp(t, 0.0, 1.0);

    if (pImpl->m_type == KnobType::Float) {
        double v1 = kf1.value.toDouble();
        double v2 = kf2.value.toDouble();
        return QVariant(v1 + (v2 - v1) * t);
    } else if (pImpl->m_type == KnobType::Integer) {
        int v1 = kf1.value.toInt();
        int v2 = kf2.value.toInt();
        return QVariant(static_cast<int>(v1 + (v2 - v1) * t));
    }

    return pImpl->m_value;
}

void AbstractKnob::clampValue()
{
    if (pImpl->m_type == KnobType::Float) {
        double val = pImpl->m_value.toDouble();
        double minVal = pImpl->m_minValue.toDouble();
        double maxVal = pImpl->m_maxValue.toDouble();
        pImpl->m_value = QVariant(std::clamp(val, minVal, maxVal));
    } else if (pImpl->m_type == KnobType::Integer) {
        int val = pImpl->m_value.toInt();
        int minVal = pImpl->m_minValue.toInt();
        int maxVal = pImpl->m_maxValue.toInt();
        pImpl->m_value = QVariant(std::clamp(val, minVal, maxVal));
    }
}

bool AbstractKnob::isValueInRange(const QVariant& value) const
{
    if (pImpl->m_type == KnobType::Float) {
        double val = value.toDouble();
        return val >= pImpl->m_minValue.toDouble() && val <= pImpl->m_maxValue.toDouble();
    } else if (pImpl->m_type == KnobType::Integer) {
        int val = value.toInt();
        return val >= pImpl->m_minValue.toInt() && val <= pImpl->m_maxValue.toInt();
    }
    return true;
}

}