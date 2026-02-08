module;

#include <algorithm>
#include <cmath>
#include <QVariant>
#include <QColor>

module Property.Abstract;

import Time.Rational;

namespace ArtifactCore {

class AbstractProperty::Impl {
public:
    QString m_name;
    PropertyType m_type;
    QVariant m_value;
    QVariant m_defaultValue;
    QVariant m_minValue;
    QVariant m_maxValue;
    bool m_isAnimatable;
    std::vector<KeyFrame> m_keyFrames;
    QColor m_colorValue;

    Impl()
        : m_name("Knob")
        , m_type(PropertyType::Float)
        , m_value(0.0)
        , m_defaultValue(0.0)
        , m_minValue(0.0)
        , m_maxValue(100.0)
        , m_isAnimatable(true)
        , m_colorValue(Qt::white)
    {
    }
};

AbstractProperty::AbstractProperty()
    : pImpl(new Impl())
{
}

AbstractProperty::~AbstractProperty()
{
    delete pImpl;
}

QString AbstractProperty::getName() const
{
    return pImpl->m_name;
}

PropertyType AbstractProperty::getType() const
{
    return pImpl->m_type;
}

QVariant AbstractProperty::getValue() const
{
    return pImpl->m_value;
}

QVariant AbstractProperty::getDefaultValue() const
{
    return pImpl->m_defaultValue;
}

QVariant AbstractProperty::getMinValue() const
{
    return pImpl->m_minValue;
}

QVariant AbstractProperty::getMaxValue() const
{
    return pImpl->m_maxValue;
}

bool AbstractProperty::isAnimatable() const
{
    return pImpl->m_isAnimatable;
}

QColor AbstractProperty::getColorValue() const
{
    return pImpl->m_colorValue;
}

void AbstractProperty::setName(const QString& name)
{
    if (pImpl->m_name != name) {
        pImpl->m_name = name;
    }
}

void AbstractProperty::setType(PropertyType type)
{
    if (pImpl->m_type != type) {
        pImpl->m_type = type;
        pImpl->m_keyFrames.clear();
    }
}

void AbstractProperty::setValue(const QVariant& value)
{
    if (pImpl->m_value != value) {
        pImpl->m_value = value;
        clampValue();
    }
}

void AbstractProperty::setDefaultValue(const QVariant& value)
{
    pImpl->m_defaultValue = value;
}

void AbstractProperty::setMinValue(const QVariant& value)
{
    pImpl->m_minValue = value;
    clampValue();
}

void AbstractProperty::setMaxValue(const QVariant& value)
{
    pImpl->m_maxValue = value;
    clampValue();
}

void AbstractProperty::setAnimatable(bool animatable)
{
    pImpl->m_isAnimatable = animatable;
    if (!animatable) {
        pImpl->m_keyFrames.clear();
    }
}

void AbstractProperty::setColorValue(const QColor& color)
{
    if (pImpl->m_colorValue != color) {
        pImpl->m_colorValue = color;
        pImpl->m_value = color;
    }
}

void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value)
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

void AbstractProperty::removeKeyFrame(const RationalTime& time)
{
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
                          [&time](const KeyFrame& kf) { return kf.time == time; });

    if (it != pImpl->m_keyFrames.end()) {
        pImpl->m_keyFrames.erase(it);
    }
}

void AbstractProperty::clearKeyFrames()
{
    pImpl->m_keyFrames.clear();
}

std::vector<KeyFrame> AbstractProperty::getKeyFrames() const
{
    return pImpl->m_keyFrames;
}

bool AbstractProperty::hasKeyFrameAt(const RationalTime& time) const
{
    return std::any_of(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
                      [&time](const KeyFrame& kf) { return kf.time == time; });
}

QVariant AbstractProperty::interpolateValue(const RationalTime& time) const
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

    if (pImpl->m_type == PropertyType::Float) {
        double v1 = kf1.value.toDouble();
        double v2 = kf2.value.toDouble();
        return QVariant(v1 + (v2 - v1) * t);
    } else if (pImpl->m_type == PropertyType::Integer) {
        int v1 = kf1.value.toInt();
        int v2 = kf2.value.toInt();
        return QVariant(static_cast<int>(v1 + (v2 - v1) * t));
    }

    return pImpl->m_value;
}

void AbstractProperty::clampValue()
{
    if (pImpl->m_type == PropertyType::Float) {
        double val = pImpl->m_value.toDouble();
        double minVal = pImpl->m_minValue.toDouble();
        double maxVal = pImpl->m_maxValue.toDouble();
        pImpl->m_value = QVariant(std::clamp(val, minVal, maxVal));
    } else if (pImpl->m_type == PropertyType::Integer) {
        int val = pImpl->m_value.toInt();
        int minVal = pImpl->m_minValue.toInt();
        int maxVal = pImpl->m_maxValue.toInt();
        pImpl->m_value = QVariant(std::clamp(val, minVal, maxVal));
    }
}

bool AbstractProperty::isValueInRange(const QVariant& value) const
{
    if (pImpl->m_type == PropertyType::Float) {
        double val = value.toDouble();
        return val >= pImpl->m_minValue.toDouble() && val <= pImpl->m_maxValue.toDouble();
    } else if (pImpl->m_type == PropertyType::Integer) {
        int val = value.toInt();
        return val >= pImpl->m_minValue.toInt() && val <= pImpl->m_maxValue.toInt();
    }
    return true;
}

}