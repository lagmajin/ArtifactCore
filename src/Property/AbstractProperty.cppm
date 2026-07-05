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
    std::vector<EnvelopeTrack> m_envelopes;

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
    for (const auto& envelope : pImpl->m_envelopes) {
        if (!envelope.enabled) {
            continue;
        }
        if (!envelope.targetPropertyPath.isEmpty() && envelope.targetPropertyPath != pImpl->m_name) {
            continue;
        }

        const double t = time.toDouble();
        const double start = envelope.startTime.toDouble();
        const double end = envelope.endTime.toDouble();

        double alpha = 0.0;
        if (end > start) {
            switch (envelope.scope) {
            case EnvelopeScope::Absolute:
            case EnvelopeScope::Entry:
                alpha = std::clamp((t - start) / (end - start), 0.0, 1.0);
                break;
            case EnvelopeScope::Exit:
                alpha = std::clamp((end - t) / (end - start), 0.0, 1.0);
                break;
            }
        } else {
            switch (envelope.scope) {
            case EnvelopeScope::Absolute:
            case EnvelopeScope::Entry:
                alpha = t >= start ? 1.0 : 0.0;
                break;
            case EnvelopeScope::Exit:
                alpha = t <= start ? 1.0 : 0.0;
                break;
            }
        }

        const double strength = std::clamp(envelope.strength, 0.0, 1.0);
        const double factor = std::clamp(alpha * strength, 0.0, 1.0);

        switch (envelope.mode) {
        case EnvelopeMode::Override:
            if (pImpl->m_type == PropertyType::Float || pImpl->m_type == PropertyType::Integer) {
                const double startValue = pImpl->m_defaultValue.isValid() ? pImpl->m_defaultValue.toDouble() : baseValue.toDouble();
                const double current = baseValue.toDouble();
                baseValue = QVariant(startValue + (current - startValue) * (1.0 - factor));
            }
            break;
        case EnvelopeMode::Add:
            if (pImpl->m_type == PropertyType::Float) {
                baseValue = QVariant(baseValue.toDouble() + factor);
            } else if (pImpl->m_type == PropertyType::Integer) {
                baseValue = QVariant(baseValue.toInt() + static_cast<int>(std::round(factor)));
            }
            break;
        case EnvelopeMode::Multiply:
            if (pImpl->m_type == PropertyType::Float) {
                baseValue = QVariant(baseValue.toDouble() * (1.0 - factor + factor * strength));
            } else if (pImpl->m_type == PropertyType::Integer) {
                baseValue = QVariant(static_cast<int>(std::round(baseValue.toInt() * (1.0 - factor + factor * strength))));
            }
            break;
        }
    }
    if (evaluator && hasExpression()) {
        try {
            // AE-like context injection
            const double frameRate = static_cast<double>(time.scale());
            evaluator->setFrameRate(frameRate);
            evaluator->setVariable("value", qvariantToExpressionValue(baseValue, pImpl->m_type));
            evaluator->setVariable("time", ExpressionValue(time.toDouble()));
            evaluator->setVariable("frameRate", ExpressionValue(frameRate));

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

void AbstractProperty::addEnvelope(const EnvelopeTrack& envelope) {
    pImpl->m_envelopes.push_back(envelope);
}

void AbstractProperty::addEnvelopePreset(const EnvelopePreset& preset) {
    for (const auto& track : preset.tracks) {
        EnvelopeTrack applied = track;
        if (!preset.targetPropertyPath.isEmpty() && applied.targetPropertyPath.isEmpty()) {
            applied.targetPropertyPath = preset.targetPropertyPath;
        }
        pImpl->m_envelopes.push_back(applied);
    }
}

void AbstractProperty::clearEnvelopes() {
    pImpl->m_envelopes.clear();
}

std::vector<EnvelopeTrack> AbstractProperty::getEnvelopes() const {
    return pImpl->m_envelopes;
}

bool AbstractProperty::hasEnvelopes() const {
    return !pImpl->m_envelopes.empty();
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
    addKeyFrame(time, value, interpolation, cp1_x, cp1_y, cp2_x, cp2_y, false);
}

void AbstractProperty::addKeyFrame(const RationalTime& time, const QVariant& value, InterpolationType interpolation,
                                    float cp1_x, float cp1_y, float cp2_x, float cp2_y, bool roving) {
    for (auto& kf : pImpl->m_keyFrames) {
        if (kf.time == time) {
            kf.value = value;
            kf.interpolation = interpolation;
            kf.cp1_x = cp1_x;
            kf.cp1_y = cp1_y;
            kf.cp2_x = cp2_x;
            kf.cp2_y = cp2_y;
            kf.roving = roving;
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
    kf.roving = roving;
    kf.anchor = KeyFrame::Anchor::Absolute;
    kf.colorLabel = KeyFrame::ColorLabel::None;
    pImpl->m_keyFrames.push_back(kf);
    std::sort(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [](const KeyFrame& a, const KeyFrame& b) {
            return a.time < b.time;
        });
}

void AbstractProperty::setKeyFrameAnchorAt(const RationalTime& time, KeyFrame::Anchor anchor) {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    if (it != pImpl->m_keyFrames.end()) {
        it->anchor = anchor;
    }
}

KeyFrame::Anchor AbstractProperty::getKeyFrameAnchorAt(const RationalTime& time) const {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    return it != pImpl->m_keyFrames.end() ? it->anchor : KeyFrame::Anchor::Absolute;
}

void AbstractProperty::setKeyFrameColorLabelAt(const RationalTime& time,
                                               KeyFrame::ColorLabel label) {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    if (it != pImpl->m_keyFrames.end()) {
        it->colorLabel = label;
    }
}

KeyFrame::ColorLabel AbstractProperty::getKeyFrameColorLabelAt(
    const RationalTime& time) const {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    return it != pImpl->m_keyFrames.end() ? it->colorLabel : KeyFrame::ColorLabel::None;
}

void AbstractProperty::retimeKeyFramesForLayerPointChange(const RationalTime& oldInPoint,
                                                          const RationalTime& oldOutPoint,
                                                          const RationalTime& newInPoint,
                                                          const RationalTime& newOutPoint) {
    const double oldIn = oldInPoint.toDouble();
    const double oldOut = oldOutPoint.toDouble();
    const double newIn = newInPoint.toDouble();
    const double newOut = newOutPoint.toDouble();
    const double oldSpan = oldOut - oldIn;
    const double newSpan = newOut - newIn;

    for (auto& kf : pImpl->m_keyFrames) {
        const double t = kf.time.toDouble();
        double newTime = t;
        switch (kf.anchor) {
        case KeyFrame::Anchor::Absolute:
            break;
        case KeyFrame::Anchor::LockToIn:
            newTime = newIn + (t - oldIn);
            break;
        case KeyFrame::Anchor::LockToOut:
            newTime = newOut + (t - oldOut);
            break;
        case KeyFrame::Anchor::StretchWithLayer:
            if (std::abs(oldSpan) > 1e-12) {
                const double normalized = (t - oldIn) / oldSpan;
                newTime = newIn + normalized * newSpan;
            }
            break;
        }
        kf.time = RationalTime::fromSeconds(newTime);
    }

    std::sort(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [](const KeyFrame& a, const KeyFrame& b) { return a.time < b.time; });
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

bool AbstractProperty::setKeyFrameRovingAt(const RationalTime& time, bool roving) {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    if (it == pImpl->m_keyFrames.end()) {
        return false;
    }
    it->roving = roving;
    return true;
}

bool AbstractProperty::getKeyFrameRovingAt(const RationalTime& time) const {
    auto it = std::find_if(pImpl->m_keyFrames.begin(), pImpl->m_keyFrames.end(),
        [&time](const KeyFrame& kf) { return kf.time == time; });
    if (it == pImpl->m_keyFrames.end()) {
        return false;
    }
    return it->roving;
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

    if (pImpl->m_type == PropertyType::Color) {
        auto interpolateColor = [](const QColor& start, const QColor& end,
                                   float alpha, InterpolationType type,
                                   float cp1_x, float cp1_y, float cp2_x,
                                   float cp2_y) {
            auto blendChannel = [&](float a, float b) {
                if (type == InterpolationType::Bezier) {
                    return bezierInterpolate(a, b, alpha, cp1_x, cp1_y, cp2_x, cp2_y);
                }
                return interpolate(a, b, alpha, type);
            };

            return QColor::fromRgbF(
                blendChannel(start.redF(), end.redF()),
                blendChannel(start.greenF(), end.greenF()),
                blendChannel(start.blueF(), end.blueF()),
                blendChannel(start.alphaF(), end.alphaF()));
        };

        for (size_t i = 0; i + 1 < pImpl->m_keyFrames.size(); ++i) {
            const auto& kf1 = pImpl->m_keyFrames[i];
            const auto& kf2 = pImpl->m_keyFrames[i + 1];
            if (time >= kf1.time && time <= kf2.time) {
                if (kf1.interpolation == InterpolationType::Constant) {
                    return kf1.value;
                }
                const QColor start = kf1.value.value<QColor>();
                const QColor end = kf2.value.value<QColor>();
                if (!start.isValid() || !end.isValid()) {
                    return kf1.value;
                }
                const double duration = kf2.time.toDouble() - kf1.time.toDouble();
                if (duration <= 0.0) {
                    return kf1.value;
                }
                const float alpha =
                    static_cast<float>((targetTime - kf1.time.toDouble()) / duration);
                return QVariant::fromValue(interpolateColor(
                    start, end, alpha, kf1.interpolation, kf1.cp1_x, kf1.cp1_y,
                    kf1.cp2_x, kf1.cp2_y));
            }
        }
        return pImpl->m_value;
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

EnvelopePreset makeBlurInEnvelopePreset(QString targetPropertyPath,
                                        RationalTime startTime,
                                        RationalTime endTime,
                                        double strength)
{
    return EnvelopePreset::make(
        QStringLiteral("Blur In"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeEntry(QString{}, startTime, endTime, EnvelopeMode::Override, strength)
        });
}

EnvelopePreset makeBlurOutEnvelopePreset(QString targetPropertyPath,
                                         RationalTime startTime,
                                         RationalTime endTime,
                                         double strength)
{
    return EnvelopePreset::make(
        QStringLiteral("Blur Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeExit(QString{}, startTime, endTime, EnvelopeMode::Override, strength)
        });
}

EnvelopePreset makeSaturationOutEnvelopePreset(QString targetPropertyPath,
                                               RationalTime startTime,
                                               RationalTime endTime,
                                               double strength)
{
    return EnvelopePreset::make(
        QStringLiteral("Fade Saturation Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeExit(QString{}, startTime, endTime, EnvelopeMode::Multiply, strength)
        });
}

EnvelopePreset makeGlowInEnvelopePreset(QString targetPropertyPath,
                                        RationalTime startTime,
                                        RationalTime endTime,
                                        double strength)
{
    return EnvelopePreset::make(
        QStringLiteral("Glow In"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeEntry(QString{}, startTime, endTime, EnvelopeMode::Add, strength)
        });
}

EnvelopePreset makeExposureOutEnvelopePreset(QString targetPropertyPath,
                                             RationalTime startTime,
                                             RationalTime endTime,
                                             double strength)
{
    return EnvelopePreset::make(
        QStringLiteral("Exposure Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeExit(QString{}, startTime, endTime, EnvelopeMode::Multiply, strength)
        });
}

EnvelopePreset makeBlurInOutEnvelopePreset(QString targetPropertyPath,
                                           RationalTime inStartTime,
                                           RationalTime inEndTime,
                                           RationalTime outStartTime,
                                           RationalTime outEndTime,
                                           double inStrength,
                                           double outStrength)
{
    return EnvelopePreset::make(
        QStringLiteral("Blur In/Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeEntry(QString{}, inStartTime, inEndTime, EnvelopeMode::Override, inStrength),
            EnvelopeTrack::makeExit(QString{}, outStartTime, outEndTime, EnvelopeMode::Override, outStrength)
        });
}

EnvelopePreset makeGlowInOutEnvelopePreset(QString targetPropertyPath,
                                           RationalTime inStartTime,
                                           RationalTime inEndTime,
                                           RationalTime outStartTime,
                                           RationalTime outEndTime,
                                           double inStrength,
                                           double outStrength)
{
    return EnvelopePreset::make(
        QStringLiteral("Glow In/Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeEntry(QString{}, inStartTime, inEndTime, EnvelopeMode::Add, inStrength),
            EnvelopeTrack::makeExit(QString{}, outStartTime, outEndTime, EnvelopeMode::Multiply, outStrength)
        });
}

EnvelopePreset makeExposureInOutEnvelopePreset(QString targetPropertyPath,
                                               RationalTime inStartTime,
                                               RationalTime inEndTime,
                                               RationalTime outStartTime,
                                               RationalTime outEndTime,
                                               double inStrength,
                                               double outStrength)
{
    return EnvelopePreset::make(
        QStringLiteral("Exposure In/Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeEntry(QString{}, inStartTime, inEndTime, EnvelopeMode::Override, inStrength),
            EnvelopeTrack::makeExit(QString{}, outStartTime, outEndTime, EnvelopeMode::Multiply, outStrength)
        });
}

EnvelopePreset makeOpacityFadeInOutEnvelopePreset(QString targetPropertyPath,
                                                  RationalTime inStartTime,
                                                  RationalTime inEndTime,
                                                  RationalTime outStartTime,
                                                  RationalTime outEndTime,
                                                  double inStrength,
                                                  double outStrength)
{
    return EnvelopePreset::make(
        QStringLiteral("Opacity Fade In/Out"),
        std::move(targetPropertyPath),
        {
            EnvelopeTrack::makeEntry(QString{}, inStartTime, inEndTime, EnvelopeMode::Multiply, inStrength),
            EnvelopeTrack::makeExit(QString{}, outStartTime, outEndTime, EnvelopeMode::Multiply, outStrength)
        });
}

} // namespace ArtifactCore
