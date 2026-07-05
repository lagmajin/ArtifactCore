module;
#include <algorithm>
#include <utility>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

module Core.Composition.FinalEffect;

namespace ArtifactCore {

// ============================================================
// CompositionFinalEffect
// ============================================================

CompositionFinalEffect::CompositionFinalEffect()
    : type_(FinalEffectType::Custom)
    , name_("Custom Effect")
    , enabled_(true)
    , opacity_(1.0f) {
}

CompositionFinalEffect::CompositionFinalEffect(FinalEffectType type)
    : type_(type)
    , name_(getTypeName(type))
    , enabled_(true)
    , opacity_(1.0f) {
}

auto CompositionFinalEffect::getParameter(const QString& key, float defaultValue) const -> float {
    auto it = parameters_.find(key);
    return (it != parameters_.end()) ? it->second : defaultValue;
}

auto CompositionFinalEffect::toJson() const -> QJsonObject {
    QJsonObject obj;
    obj["type"] = static_cast<int>(type_);
    obj["name"] = name_;
    obj["enabled"] = enabled_;
    obj["opacity"] = opacity_;

    QJsonArray params;
    for (const auto& [key, value] : parameters_) {
        QJsonObject param;
        param["key"] = key;
        param["value"] = value;
        params.append(param);
    }
    obj["parameters"] = params;

    return obj;
}

void CompositionFinalEffect::fromJson(const QJsonObject& obj) {
    if (obj.contains("type")) {
        type_ = static_cast<FinalEffectType>(obj["type"].toInt());
    }
    if (obj.contains("name")) {
        name_ = obj["name"].toString();
    }
    if (obj.contains("enabled")) {
        enabled_ = obj["enabled"].toBool();
    }
    if (obj.contains("opacity")) {
        opacity_ = static_cast<float>(obj["opacity"].toDouble());
    }
    if (obj.contains("parameters") && obj["parameters"].isArray()) {
        parameters_.clear();
        auto arr = obj["parameters"].toArray();
        for (const auto& val : arr) {
            auto paramObj = val.toObject();
            if (paramObj.contains("key") && paramObj.contains("value")) {
                parameters_[paramObj["key"].toString()] = static_cast<float>(paramObj["value"].toDouble());
            }
        }
    }
}

auto CompositionFinalEffect::createFromType(FinalEffectType type) -> std::unique_ptr<CompositionFinalEffect> {
    auto effect = std::make_unique<CompositionFinalEffect>(type);
    return effect;
}

auto CompositionFinalEffect::createFromJson(const QJsonObject& obj) -> std::unique_ptr<CompositionFinalEffect> {
    auto effect = std::make_unique<CompositionFinalEffect>();
    effect->fromJson(obj);
    return effect;
}

auto CompositionFinalEffect::getTypeName(FinalEffectType type) -> QString {
    switch (type) {
        case FinalEffectType::ColorGrading: return "Color Grading";
        case FinalEffectType::ColorCurves: return "Color Curves";
        case FinalEffectType::LUT: return "LUT";
        case FinalEffectType::Exposure: return "Exposure";
        case FinalEffectType::Glow: return "Glow";
        case FinalEffectType::Blur: return "Blur";
        case FinalEffectType::Sharpen: return "Sharpen";
        case FinalEffectType::Vignette: return "Vignette";
        case FinalEffectType::Grain: return "Grain";
        case FinalEffectType::ToneMapping: return "Tone Mapping";
        case FinalEffectType::Custom: return "Custom";
        default: return "Unknown";
    }
}

// ============================================================
// CompositionFinalEffectStack
// ============================================================

void CompositionFinalEffectStack::addEffect(std::unique_ptr<CompositionFinalEffect> effect) {
    if (!effect) return;
    effects_.push_back(std::move(effect));
}

void CompositionFinalEffectStack::removeEffect(int index) {
    if (index < 0 || index >= static_cast<int>(effects_.size())) return;
    effects_.erase(effects_.begin() + index);
}

void CompositionFinalEffectStack::removeEffect(const QString& name) {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
            [&name](const auto& effect) {
                return effect && effect->getName() == name;
            }),
        effects_.end()
    );
}

void CompositionFinalEffectStack::clear() {
    effects_.clear();
}

void CompositionFinalEffectStack::moveEffect(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= static_cast<int>(effects_.size())) return;
    if (toIndex < 0 || toIndex >= static_cast<int>(effects_.size())) return;

    auto effect = std::move(effects_[fromIndex]);
    effects_.erase(effects_.begin() + fromIndex);
    effects_.insert(effects_.begin() + toIndex, std::move(effect));
}

auto CompositionFinalEffectStack::getEffect(int index) -> CompositionFinalEffect* {
    if (index < 0 || index >= static_cast<int>(effects_.size())) return nullptr;
    return effects_[index].get();
}

auto CompositionFinalEffectStack::getEffect(int index) const -> const CompositionFinalEffect* {
    if (index < 0 || index >= static_cast<int>(effects_.size())) return nullptr;
    return effects_[index].get();
}

auto CompositionFinalEffectStack::findEffect(const QString& name) -> CompositionFinalEffect* {
    for (auto& effect : effects_) {
        if (effect && effect->getName() == name) {
            return effect.get();
        }
    }
    return nullptr;
}

auto CompositionFinalEffectStack::findEffect(const QString& name) const -> const CompositionFinalEffect* {
    for (const auto& effect : effects_) {
        if (effect && effect->getName() == name) {
            return effect.get();
        }
    }
    return nullptr;
}

auto CompositionFinalEffectStack::getEnabledEffects() const -> std::vector<const CompositionFinalEffect*> {
    std::vector<const CompositionFinalEffect*> result;
    for (const auto& effect : effects_) {
        if (effect && effect->isEnabled()) {
            result.push_back(effect.get());
        }
    }
    return result;
}

auto CompositionFinalEffectStack::toJson() const -> QJsonObject {
    QJsonObject obj;
    QJsonArray effectsArr;
    for (const auto& effect : effects_) {
        if (effect) {
            effectsArr.append(effect->toJson());
        }
    }
    obj["effects"] = effectsArr;
    return obj;
}

void CompositionFinalEffectStack::fromJson(const QJsonObject& obj) {
    clear();
    if (obj.contains("effects") && obj["effects"].isArray()) {
        auto arr = obj["effects"].toArray();
        for (const auto& val : arr) {
            auto effect = CompositionFinalEffect::createFromJson(val.toObject());
            if (effect) {
                addEffect(std::move(effect));
            }
        }
    }
}

} // namespace ArtifactCore
