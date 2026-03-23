module;

#include <algorithm>
#include <cmath>
#include <QVariant>
#include <QColor>

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
module Property.Abstract;





namespace ArtifactCore {

// -----------------------------------------------------------------------
// Impl
// -----------------------------------------------------------------------
class AbstractProperty::Impl {
public:
    QString       m_name;
    PropertyType  m_type           = PropertyType::Float;
    QVariant      m_value;
    QVariant      m_defaultValue;
    QVariant      m_minValue;
    QVariant      m_maxValue;
    bool          m_animatable     = false;
    int           m_displayPriority = 0;          ///< 表示優先度（小さい値が先頭）
    PropertyMetadata m_metadata;

    std::vector<KeyFrame> m_keyFrames;
};

// Constructor / Destructor
// -----------------------------------------------------------------------
AbstractProperty::AbstractProperty()
    : pImpl(new Impl()) {}

AbstractProperty::~AbstractProperty() {
    delete pImpl;
}

// Copy/Move constructors
AbstractProperty::AbstractProperty(const AbstractProperty& other)
    : pImpl(new Impl(*other.pImpl)) {}

AbstractProperty& AbstractProperty::operator=(const AbstractProperty& other) {
    if (this != &other) {
        delete pImpl;
        pImpl = new Impl(*other.pImpl);
    }
    return *this;
}

AbstractProperty::AbstractProperty(AbstractProperty&& other) noexcept
    : pImpl(other.pImpl) {
    other.pImpl = nullptr;
}

AbstractProperty& AbstractProperty::operator=(AbstractProperty&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

// -----------------------------------------------------------------------
// Getters
// -----------------------------------------------------------------------
QString AbstractProperty::getName() const {
    return pImpl->m_name;
}

PropertyType AbstractProperty::getType() const {
    return pImpl->m_type;
}

QVariant AbstractProperty::getValue() const {
    return pImpl->m_value;
}

QVariant AbstractProperty::getDefaultValue() const {
    return pImpl->m_defaultValue;
}

QVariant AbstractProperty::getMinValue() const {
    return pImpl->m_minValue;
}

QVariant AbstractProperty::getMaxValue() const {
    return pImpl->m_maxValue;
}

bool AbstractProperty::isAnimatable() const {
    return pImpl->m_animatable;
}

QColor AbstractProperty::getColorValue() const {
    return pImpl->m_value.value<QColor>();
}

PropertyMetadata AbstractProperty::metadata() const {
    return pImpl->m_metadata;
}

// -----------------------------------------------------------------------
// Setters
// -----------------------------------------------------------------------
void AbstractProperty::setName(const QString& name) {
    pImpl->m_name = name;
}

void AbstractProperty::setType(PropertyType type) {
    if (pImpl->m_type != type) {
        pImpl->m_type = type;
        pImpl->m_keyFrames.clear();
    }
}

void AbstractProperty::setValue(const QVariant& value) {
    pImpl->m_value = value;
    clampValue();
}

void AbstractProperty::setDefaultValue(const QVariant& value) {
    pImpl->m_defaultValue = value;
}

void AbstractProperty::setMinValue(const QVariant& value) {
    pImpl->m_minValue = value;
}

void AbstractProperty::setMaxValue(const QVariant& value) {
    pImpl->m_maxValue = value;
}

void AbstractProperty::setAnimatable(bool animatable) {
    pImpl->m_animatable = animatable;
}

void AbstractProperty::setColorValue(const QColor& color) {
    pImpl->m_value = QVariant::fromValue(color);
}

void AbstractProperty::setMetadata(const PropertyMetadata& metadata) {
    pImpl->m_metadata = metadata;
}

void AbstractProperty::setDisplayLabel(const QString& label) {
    pImpl->m_metadata.displayLabel = label;
}

void AbstractProperty::setUnit(const QString& unit) {
    pImpl->m_metadata.unit = unit;
}

void AbstractProperty::setTooltip(const QString& tooltip) {
    pImpl->m_metadata.tooltip = tooltip;
}

void AbstractProperty::setStep(const QVariant& step) {
    pImpl->m_metadata.step = step;
}

void AbstractProperty::setHardRange(const QVariant& minValue, const QVariant& maxValue) {
    pImpl->m_metadata.hardMin = minValue;
    pImpl->m_metadata.hardMax = maxValue;
}

void AbstractProperty::setSoftRange(const QVariant& minValue, const QVariant& maxValue) {
    pImpl->m_metadata.softMin = minValue;
    pImpl->m_metadata.softMax = maxValue;
}

// -----------------------------------------------------------------------
// 表示優先度
// -----------------------------------------------------------------------
void AbstractProperty::setDisplayPriority(int priority) {
    pImpl->m_displayPriority = priority;
}

int AbstractProperty::displayPriority() const {
    return pImpl->m_displayPriority;
}

// -----------------------------------------------------------------------
// KeyFrame operations
// -----------------------------------------------------------------------
void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value) {
    addKeyFrame(time, value, EasingType::Linear);
}

void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value, EasingType easing) {
    // 同じ時刻のキーフレームがあれば更新
    for (auto& kf : pImpl->m_keyFrames) {
        if (kf.time == time) {
            kf.value = value;
            kf.easing = easing;
            return;
        }
    }
    pImpl->m_keyFrames.push_back({ time, value, easing });
    // 時刻順にソート
    std::sort(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [](const KeyFrame& a, const KeyFrame& b) {
            return a.time < b.time;
        });
}

void AbstractProperty::removeKeyFrame(const RationalTime& time) {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    if (it != pImpl->m_keyFrames.end()) {
        pImpl->m_keyFrames.erase(it);
    }
}

void AbstractProperty::clearKeyFrames() {
    pImpl->m_keyFrames.clear();
}

std::vector<KeyFrame> AbstractProperty::getKeyFrames() const {
    return pImpl->m_keyFrames;
}

bool AbstractProperty::hasKeyFrameAt(const RationalTime& time) const {
    return std::any_of(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
}

QVariant AbstractProperty::interpolateValue(const RationalTime& time) const {
    if (pImpl->m_keyFrames.empty()) {
        return pImpl->m_value;
    }
    if (pImpl->m_keyFrames.size() == 1) {
        return pImpl->m_keyFrames.front().value;
    }

    // 範囲外クランプ
    if (time <= pImpl->m_keyFrames.front().time) {
        return pImpl->m_keyFrames.front().value;
    }
    if (time >= pImpl->m_keyFrames.back().time) {
        return pImpl->m_keyFrames.back().value;
    }

    // 前後キーフレームを線形補間
    for (size_t i = 0; i + 1 < pImpl->m_keyFrames.size(); ++i) {
        const auto& kf1 = pImpl->m_keyFrames[i];
        const auto& kf2 = pImpl->m_keyFrames[i + 1];
        if (time >= kf1.time && time <= kf2.time) {
            double elapsed  = (time  - kf1.time).toDouble();
            double duration = (kf2.time - kf1.time).toDouble();
            double t = (duration > 0.0) ? (elapsed / duration) : 0.0;

            if (kf1.easing == EasingType::Hold) {
                return kf1.value;
            }

            if (pImpl->m_type == PropertyType::Float) {
                double v1 = kf1.value.toDouble();
                double v2 = kf2.value.toDouble();
                return QVariant(lerp(v1, v2, t));
            }
            if (pImpl->m_type == PropertyType::Integer) {
                int v1 = kf1.value.toInt();
                int v2 = kf2.value.toInt();
                return QVariant(static_cast<int>(std::round(lerp(v1, v2, t))));
            }
            // それ以外はステップ補間
            return kf1.value;
        }
    }
    return pImpl->m_value;
}

// -----------------------------------------------------------------------
// Validation
// -----------------------------------------------------------------------
void AbstractProperty::clampValue() {
    if (!pImpl->m_minValue.isValid() && !pImpl->m_maxValue.isValid()) return;

    if (pImpl->m_type == PropertyType::Float) {
        double val = pImpl->m_value.toDouble();
        if (pImpl->m_minValue.isValid()) val = std::max(val, pImpl->m_minValue.toDouble());
        if (pImpl->m_maxValue.isValid()) val = std::min(val, pImpl->m_maxValue.toDouble());
        pImpl->m_value = QVariant(val);
    } else if (pImpl->m_type == PropertyType::Integer) {
        int val = pImpl->m_value.toInt();
        if (pImpl->m_minValue.isValid()) val = std::max(val, pImpl->m_minValue.toInt());
        if (pImpl->m_maxValue.isValid()) val = std::min(val, pImpl->m_maxValue.toInt());
        pImpl->m_value = QVariant(val);
    }
}

bool AbstractProperty::isValueInRange(const QVariant& value) const {
    if (pImpl->m_type == PropertyType::Float) {
        double val = value.toDouble();
        if (pImpl->m_minValue.isValid() && val < pImpl->m_minValue.toDouble()) return false;
        if (pImpl->m_maxValue.isValid() && val > pImpl->m_maxValue.toDouble()) return false;
        return true;
    }
    if (pImpl->m_type == PropertyType::Integer) {
        int val = value.toInt();
        if (pImpl->m_minValue.isValid() && val < pImpl->m_minValue.toInt()) return false;
        if (pImpl->m_maxValue.isValid() && val > pImpl->m_maxValue.toInt()) return false;
        return true;
    }
    return true;
}

} // namespace ArtifactCore
