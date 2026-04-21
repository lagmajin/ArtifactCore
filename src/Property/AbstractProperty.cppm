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

import Script.Expression.Evaluator;
import Script.Expression.Value;
import Math.Interpolate;





namespace ArtifactCore {

// -----------------------------------------------------------------------
// Conversion Helpers
// -----------------------------------------------------------------------
namespace {
    ExpressionValue qvariantToExpressionValue(const QVariant& v, PropertyType type) {
        if (!v.isValid()) return ExpressionValue();
        switch (type) {
            case PropertyType::Float: return ExpressionValue(v.toDouble());
            case PropertyType::Integer: return ExpressionValue(v.toDouble()); 
            case PropertyType::Boolean: return ExpressionValue(v.toBool() ? 1.0 : 0.0);
            case PropertyType::String: return ExpressionValue(v.toString().toStdString());
            case PropertyType::Color: {
                QColor c = v.value<QColor>();
                return ExpressionValue(c.redF(), c.greenF(), c.blueF(), c.alphaF());
            }
            default: return ExpressionValue();
        }
    }

    QVariant expressionValueToQVariant(const ExpressionValue& ev, PropertyType type) {
        switch (type) {
            case PropertyType::Float: return QVariant(ev.asNumber());
            case PropertyType::Integer: return QVariant(static_cast<int>(std::round(ev.asNumber())));
            case PropertyType::Boolean: return QVariant(ev.asNumber() > 0.5);
            case PropertyType::String: return QVariant(QString::fromStdString(ev.asString()));
            case PropertyType::Color: {
                if (ev.isVector() || ev.isArray()) {
                     return QVariant::fromValue(QColor::fromRgbF(
                         static_cast<float>(ev.x()),
                         static_cast<float>(ev.y()),
                         static_cast<float>(ev.z()),
                         static_cast<float>(ev.length() > 3 ? ev.w() : 1.0)
                     ));
                }
                return QColor(Qt::white);
            }
            default: return QVariant();
        }
    }
}

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

    QString m_expression;
    std::vector<KeyFrame> m_keyFrames;

    QVariant m_externalOverride;
    bool     m_hasExternalOverride = false;
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
// Expression support
// -----------------------------------------------------------------------
void AbstractProperty::setExpression(const QString& expression) {
    pImpl->m_expression = expression;
}

QString AbstractProperty::getExpression() const {
    return pImpl->m_expression;
}

bool AbstractProperty::hasExpression() const {
    return !pImpl->m_expression.isEmpty();
}

QVariant AbstractProperty::evaluateValue(const RationalTime& time, ExpressionEvaluator* evaluator) const {
    if (pImpl->m_hasExternalOverride) {
        return pImpl->m_externalOverride;
    }

    QVariant baseValue = interpolateValue(time);
    if (evaluator && hasExpression()) {
        try {
            // AE-like context injection
            evaluator->setVariable("value", qvariantToExpressionValue(baseValue, pImpl->m_type));
            evaluator->setVariable("time", ExpressionValue(time.toDouble()));
            
            ExpressionValue result = evaluator->evaluate(pImpl->m_expression.toStdString());
            if (!evaluator->hasError()) {
                return expressionValueToQVariant(result, pImpl->m_type);
            } else {
                // If error, return baseValue as fallback
                std::cerr << "Expression error in property " << pImpl->m_name.toStdString() 
                          << ": " << evaluator->getError() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Expression exception in property " << pImpl->m_name.toStdString() 
                      << ": " << e.what() << std::endl;
        }
    }
    return baseValue;
}

// -----------------------------------------------------------------------
// KeyFrame operations
// -----------------------------------------------------------------------
void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value) {
    addKeyFrame(time, value, InterpolationType::Linear);
}

void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value, InterpolationType interpolation) {
    addKeyFrame(time, value, interpolation, 0.42f, 0.0f, 0.58f, 1.0f);
}

void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value, InterpolationType interpolation,
                                    float cp1_x, float cp1_y, float cp2_x, float cp2_y) {
    for (auto& kf : pImpl->m_keyFrames) {
        if (kf.time == time) {
            kf.value = value;
            kf.interpolation = interpolation;
            kf.cp1_x = cp1_x;
            kf.cp1_y = cp1_y;
            kf.cp2_x = cp2_x;
            kf.cp2_y = cp2_y;
            return;
        }
    }
    KeyFrame kf;
    kf.time = time;
    kf.value = value;
    kf.interpolation = interpolation;
    kf.cp1_x = cp1_x;
    kf.cp1_y = cp1_y;
    kf.cp2_x = cp2_x;
    kf.cp2_y = cp2_y;
    pImpl->m_keyFrames.push_back(kf);
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

    if (time <= pImpl->m_keyFrames.front().time) {
        return pImpl->m_keyFrames.front().value;
    }
    if (time >= pImpl->m_keyFrames.back().time) {
        return pImpl->m_keyFrames.back().value;
    }

    const double targetTime = time.toDouble();

    if (pImpl->m_type == PropertyType::Float) {
        KeyframeInterpolator<float> interp;
        for (const auto& kf : pImpl->m_keyFrames) {
            typename KeyframeInterpolator<float>::KeyframeEntry entry;
            entry.time = kf.time.toDouble();
            entry.value = kf.value.toFloat();
            entry.type = kf.interpolation;
            entry.cp1_x = kf.cp1_x;
            entry.cp1_y = kf.cp1_y;
            entry.cp2_x = kf.cp2_x;
            entry.cp2_y = kf.cp2_y;
            interp.addKeyframe(entry);
        }
        return QVariant(interp.evaluate(targetTime));
    }

    if (pImpl->m_type == PropertyType::Integer) {
        KeyframeInterpolator<float> interp;
        for (const auto& kf : pImpl->m_keyFrames) {
            typename KeyframeInterpolator<float>::KeyframeEntry entry;
            entry.time = kf.time.toDouble();
            entry.value = static_cast<float>(kf.value.toInt());
            entry.type = kf.interpolation;
            entry.cp1_x = kf.cp1_x;
            entry.cp1_y = kf.cp1_y;
            entry.cp2_x = kf.cp2_x;
            entry.cp2_y = kf.cp2_y;
            interp.addKeyframe(entry);
        }
        return QVariant(static_cast<int>(std::round(interp.evaluate(targetTime))));
    }

    for (size_t i = 0; i + 1 < pImpl->m_keyFrames.size(); ++i) {
        const auto& kf1 = pImpl->m_keyFrames[i];
        const auto& kf2 = pImpl->m_keyFrames[i + 1];
        if (time >= kf1.time && time <= kf2.time) {
            if (kf1.interpolation == InterpolationType::Constant) {
                return kf1.value;
            }
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

void AbstractProperty::setExternalOverride(const QVariant& value) {
    pImpl->m_externalOverride = value;
    pImpl->m_hasExternalOverride = true;
}

void AbstractProperty::clearExternalOverride() {
    pImpl->m_hasExternalOverride = false;
    pImpl->m_externalOverride = QVariant();
}

bool AbstractProperty::hasExternalOverride() const {
    return pImpl->m_hasExternalOverride;
}

void AbstractProperty::setExternalNormalizedValue(double normalized) {
    if (pImpl->m_type == PropertyType::Float) {
        double min = pImpl->m_minValue.isValid() ? pImpl->m_minValue.toDouble() : 0.0;
        double max = pImpl->m_maxValue.isValid() ? pImpl->m_maxValue.toDouble() : 1.0;
        
        // Use soft range if available, as it's often more appropriate for controllers
        if (pImpl->m_metadata.softMin.isValid() && pImpl->m_metadata.softMax.isValid()) {
            min = pImpl->m_metadata.softMin.toDouble();
            max = pImpl->m_metadata.softMax.toDouble();
        }
        
        setExternalOverride(min + (max - min) * normalized);
    } else if (pImpl->m_type == PropertyType::Integer) {
        int min = pImpl->m_minValue.isValid() ? pImpl->m_minValue.toInt() : 0;
        int max = pImpl->m_maxValue.isValid() ? pImpl->m_maxValue.toInt() : 100;
        setExternalOverride(static_cast<int>(std::round(min + (max - min) * normalized)));
    } else if (pImpl->m_type == PropertyType::Boolean) {
        setExternalOverride(normalized > 0.5);
    }
}

} // namespace ArtifactCore
