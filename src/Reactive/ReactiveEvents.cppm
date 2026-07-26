module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QUuid>

module Reactive.Events;

namespace ArtifactCore {

// ============================================================
// String Conversion
// ============================================================

const char* triggerEventTypeName(TriggerEventType type) {
    switch (type) {
        case TriggerEventType::None:          return "None";
        case TriggerEventType::OnStart:       return "OnStart";
        case TriggerEventType::OnEnd:         return "OnEnd";
        case TriggerEventType::OnEnterRange:  return "OnEnterRange";
        case TriggerEventType::OnExitRange:   return "OnExitRange";
        case TriggerEventType::OnLoop:        return "OnLoop";
        case TriggerEventType::OnContact:     return "OnContact";
        case TriggerEventType::OnSeparation:  return "OnSeparation";
        case TriggerEventType::OnProximity:   return "OnProximity";
        case TriggerEventType::OnValueExceed: return "OnValueExceed";
        case TriggerEventType::OnValueDrop:   return "OnValueDrop";
        case TriggerEventType::OnValueCross:  return "OnValueCross";
        case TriggerEventType::OnFrame:       return "OnFrame";
        default: return "None";
    }
}

const char* reactionTypeName(ReactionType type) {
    switch (type) {
        case ReactionType::None:              return "None";
        case ReactionType::SetProperty:       return "SetProperty";
        case ReactionType::AnimateProperty:   return "AnimateProperty";
        case ReactionType::RandomizeProperty: return "RandomizeProperty";
        case ReactionType::ApplyImpulse:      return "ApplyImpulse";
        case ReactionType::ApplyForce:        return "ApplyForce";
        case ReactionType::Attract:           return "Attract";
        case ReactionType::Repel:             return "Repel";
        case ReactionType::PlayAnimation:     return "PlayAnimation";
        case ReactionType::PauseAnimation:    return "PauseAnimation";
        case ReactionType::GoToFrame:         return "GoToFrame";
        case ReactionType::SpawnLayer:        return "SpawnLayer";
        case ReactionType::DestroyLayer:      return "DestroyLayer";
        case ReactionType::PlaySound:         return "PlaySound";
        default: return "None";
    }
}

TriggerEventType triggerEventTypeFromName(const QString& name) {
    if (name == "OnStart")       return TriggerEventType::OnStart;
    if (name == "OnEnd")         return TriggerEventType::OnEnd;
    if (name == "OnEnterRange")  return TriggerEventType::OnEnterRange;
    if (name == "OnExitRange")   return TriggerEventType::OnExitRange;
    if (name == "OnLoop")        return TriggerEventType::OnLoop;
    if (name == "OnContact")     return TriggerEventType::OnContact;
    if (name == "OnSeparation")  return TriggerEventType::OnSeparation;
    if (name == "OnProximity")   return TriggerEventType::OnProximity;
    if (name == "OnValueExceed") return TriggerEventType::OnValueExceed;
    if (name == "OnValueDrop")   return TriggerEventType::OnValueDrop;
    if (name == "OnValueCross")  return TriggerEventType::OnValueCross;
    if (name == "OnFrame")       return TriggerEventType::OnFrame;
    return TriggerEventType::None;
}

ReactionType reactionTypeFromName(const QString& name) {
    if (name == "SetProperty")       return ReactionType::SetProperty;
    if (name == "AnimateProperty")   return ReactionType::AnimateProperty;
    if (name == "RandomizeProperty") return ReactionType::RandomizeProperty;
    if (name == "ApplyImpulse")      return ReactionType::ApplyImpulse;
    if (name == "ApplyForce")        return ReactionType::ApplyForce;
    if (name == "Attract")           return ReactionType::Attract;
    if (name == "Repel")             return ReactionType::Repel;
    if (name == "PlayAnimation")     return ReactionType::PlayAnimation;
    if (name == "PauseAnimation")    return ReactionType::PauseAnimation;
    if (name == "GoToFrame")         return ReactionType::GoToFrame;
    if (name == "SpawnLayer")        return ReactionType::SpawnLayer;
    if (name == "DestroyLayer")      return ReactionType::DestroyLayer;
    if (name == "PlaySound")         return ReactionType::PlaySound;
    return ReactionType::None;
}

// ============================================================
// TriggerCondition Serialization
// ============================================================

QString TriggerCondition::toJson() const {
    QJsonObject obj;
    obj["type"] = triggerEventTypeName(type);
    obj["sourceLayerId"] = sourceLayerId;
    obj["targetLayerId"] = targetLayerId;
    obj["proximityThreshold"] = static_cast<double>(proximityThreshold);
    obj["propertyPath"] = propertyPath;
    obj["valueThreshold"] = static_cast<double>(valueThreshold);
    obj["frameNumber"] = static_cast<double>(frameNumber);
    return QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

TriggerCondition TriggerCondition::fromJson(const QString& json) {
    TriggerCondition tc;
    auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return tc;
    auto obj = doc.object();
    tc.type = triggerEventTypeFromName(obj["type"].toString());
    tc.sourceLayerId = obj["sourceLayerId"].toString();
    tc.targetLayerId = obj["targetLayerId"].toString();
    tc.proximityThreshold = static_cast<float>(obj["proximityThreshold"].toDouble(50.0));
    tc.propertyPath = obj["propertyPath"].toString();
    tc.valueThreshold = static_cast<float>(obj["valueThreshold"].toDouble(0.0));
    tc.frameNumber = static_cast<int64_t>(obj["frameNumber"].toDouble(0));
    return tc;
}

// ============================================================
// Reaction Serialization
// ============================================================

QString Reaction::toJson() const {
    QJsonObject obj;
    obj["type"] = reactionTypeName(type);
    obj["targetLayerId"] = targetLayerId;
    obj["propertyPath"] = propertyPath;
    obj["value"] = QJsonValue::fromVariant(value);
    obj["valueMax"] = QJsonValue::fromVariant(valueMax);
    obj["duration"] = static_cast<double>(duration);
    obj["easing"] = easing;
    obj["strength"] = static_cast<double>(strength);
    obj["directionX"] = static_cast<double>(directionX);
    obj["directionY"] = static_cast<double>(directionY);
    obj["targetFrame"] = static_cast<double>(targetFrame);
    obj["spawnLayerType"] = spawnLayerType;
    return QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

Reaction Reaction::fromJson(const QString& json) {
    Reaction r;
    auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return r;
    auto obj = doc.object();
    r.type = reactionTypeFromName(obj["type"].toString());
    r.targetLayerId = obj["targetLayerId"].toString();
    r.propertyPath = obj["propertyPath"].toString();
    r.value = obj["value"].toVariant();
    r.valueMax = obj["valueMax"].toVariant();
    r.duration = static_cast<float>(obj["duration"].toDouble(0.0));
    r.easing = obj["easing"].toString("easeInOut");
    r.strength = static_cast<float>(obj["strength"].toDouble(1.0));
    r.directionX = static_cast<float>(obj["directionX"].toDouble(0.0));
    r.directionY = static_cast<float>(obj["directionY"].toDouble(0.0));
    r.targetFrame = static_cast<int64_t>(obj["targetFrame"].toDouble(0));
    r.spawnLayerType = obj["spawnLayerType"].toString();
    return r;
}

// ============================================================
// ReactiveRule Serialization
// ============================================================

QString ReactiveRule::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["enabled"] = enabled;
    obj["trigger"] = QJsonDocument::fromJson(trigger.toJson().toUtf8()).object();
    QJsonArray reactionsArr;
    for (const auto& r : reactions) {
        reactionsArr.append(QJsonDocument::fromJson(r.toJson().toUtf8()).object());
    }
    obj["reactions"] = reactionsArr;
    obj["once"] = once;
    obj["repeating"] = repeating;
    obj["delay"] = static_cast<double>(delay);
    obj["cooldown"] = static_cast<double>(cooldown);
    return QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

ReactiveRule ReactiveRule::fromJson(const QString& json) {
    ReactiveRule rule;
    auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return rule;
    auto obj = doc.object();
    rule.id = obj["id"].toString();
    if (rule.id.isEmpty()) rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.name = obj["name"].toString("Unnamed Rule");
    rule.enabled = obj["enabled"].toBool(true);

    // Trigger
    auto triggerObj = obj["trigger"].toObject();
    rule.trigger = TriggerCondition::fromJson(QString(QJsonDocument(triggerObj).toJson(QJsonDocument::Compact)));

    // Reactions
    auto reactionsArr = obj["reactions"].toArray();
    for (const auto& rVal : reactionsArr) {
        rule.reactions.push_back(Reaction::fromJson(QString(QJsonDocument(rVal.toObject()).toJson(QJsonDocument::Compact))));
    }

    rule.once = obj["once"].toBool(false);
    rule.repeating = obj["repeating"].toBool(false);
    rule.delay = static_cast<float>(obj["delay"].toDouble(0.0));
    rule.cooldown = static_cast<float>(obj["cooldown"].toDouble(0.0));
    return rule;
}

QString ReactiveRule::toJsonArray(const std::vector<ReactiveRule>& rules) {
    QJsonArray arr;
    for (const auto& rule : rules) {
        arr.append(QJsonDocument::fromJson(rule.toJson().toUtf8()).object());
    }
    return QString(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

std::vector<ReactiveRule> ReactiveRule::fromJsonArray(const QString& jsonArray) {
    std::vector<ReactiveRule> rules;
    auto doc = QJsonDocument::fromJson(jsonArray.toUtf8());
    if (!doc.isArray()) return rules;
    for (const auto& val : doc.array()) {
        rules.push_back(ReactiveRule::fromJson(QString(QJsonDocument(val.toObject()).toJson(QJsonDocument::Compact))));
    }
    return rules;
}

// ============================================================
// ReactiveEngine 実装
// ============================================================

void ReactiveEngine::addRule(const ReactiveRule& rule) {
    for (auto& r : rules_) {
        if (r.id == rule.id) {
            r = rule;
            fired_.remove(rule.id);
            lastFiredFrame_.remove(rule.id);
            fireAccumulator_.remove(rule.id);
            cooldownRemaining_.remove(rule.id);
            return;
        }
    }
    rules_.push_back(rule);
}

void ReactiveEngine::removeRule(const QString& ruleId) {
    rules_.erase(std::remove_if(rules_.begin(), rules_.end(),
        [&](const auto& r) { return r.id == ruleId; }), rules_.end());
    fired_.remove(ruleId);
    lastFiredFrame_.remove(ruleId);
    fireAccumulator_.remove(ruleId);
    cooldownRemaining_.remove(ruleId);
}

void ReactiveEngine::clearRules() {
    rules_.clear();
    fired_.clear();
    lastFiredFrame_.clear();
    fireAccumulator_.clear();
    cooldownRemaining_.clear();
    prevActive_.clear();
    prevValues_.clear();
    prevFrame_ = -1;
}

size_t ReactiveEngine::ruleCount() const { return rules_.size(); }
std::vector<ReactiveRule> ReactiveEngine::rules() const { return rules_; }

std::vector<TriggerEvent> ReactiveEngine::evaluate(const ReactiveEvaluationContext& ctx) {
    std::vector<TriggerEvent> results;

    for (auto& rule : rules_) {
        if (!rule.enabled) continue;
        if (rule.once && fired_.value(rule.id, false)) continue;

        float& cd = cooldownRemaining_[rule.id];
        if (cd > 0.0f) {
            cd -= ctx.deltaTime;
            if (cd > 0.0f) continue;
            cd = 0.0f;
        }

        const auto& tc = rule.trigger;
        bool conditionMet = false;

        switch (tc.type) {
        case TriggerEventType::OnStart:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerInPoint)
                conditionMet = (ctx.currentFrame >= ctx.layerInPoint(tc.sourceLayerId))
                    && !prevActive_.value(tc.sourceLayerId, false);
            break;
        case TriggerEventType::OnEnd:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerOutPoint)
                conditionMet = (ctx.currentFrame >= ctx.layerOutPoint(tc.sourceLayerId))
                    && prevActive_.value(tc.sourceLayerId, true);
            break;
        case TriggerEventType::OnEnterRange:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerIsActive) {
                bool now = ctx.layerIsActive(tc.sourceLayerId);
                conditionMet = now && !prevActive_.value(tc.sourceLayerId, false);
            }
            break;
        case TriggerEventType::OnExitRange:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerIsActive) {
                bool now = ctx.layerIsActive(tc.sourceLayerId);
                conditionMet = !now && prevActive_.value(tc.sourceLayerId, false);
            }
            break;
        case TriggerEventType::OnLoop:
            conditionMet = (prevFrame_ >= 0) && (ctx.currentFrame < prevFrame_);
            break;
        case TriggerEventType::OnContact:
        case TriggerEventType::OnSeparation:
        case TriggerEventType::OnProximity:
            // TBD — needs layerBounds callback in evaluation context
            conditionMet = false;
            break;
        case TriggerEventType::OnValueExceed:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerPropertyValue) {
                double now = ctx.layerPropertyValue(tc.sourceLayerId, tc.propertyPath).toDouble();
                QString key = tc.sourceLayerId + "." + tc.propertyPath;
                double prev = prevValues_.value(key, now);
                conditionMet = (now > tc.valueThreshold) && (prev <= tc.valueThreshold);
                prevValues_[key] = now;
            }
            break;
        case TriggerEventType::OnValueDrop:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerPropertyValue) {
                double now = ctx.layerPropertyValue(tc.sourceLayerId, tc.propertyPath).toDouble();
                QString key = tc.sourceLayerId + "." + tc.propertyPath;
                double prev = prevValues_.value(key, now);
                conditionMet = (now < tc.valueThreshold) && (prev >= tc.valueThreshold);
                prevValues_[key] = now;
            }
            break;
        case TriggerEventType::OnValueCross:
            if (!tc.sourceLayerId.isEmpty() && ctx.layerPropertyValue) {
                double now = ctx.layerPropertyValue(tc.sourceLayerId, tc.propertyPath).toDouble();
                QString key = tc.sourceLayerId + "." + tc.propertyPath;
                double prev = prevValues_.value(key, now);
                conditionMet = ((now - tc.valueThreshold) * (prev - tc.valueThreshold)) < 0.0;
                prevValues_[key] = now;
            }
            break;
        case TriggerEventType::OnFrame:
            conditionMet = (ctx.currentFrame == tc.frameNumber);
            break;
        default:
            break;
        }

        if (!conditionMet) continue;

        // delay
        if (rule.delay > 0.0f) {
            float& acc = fireAccumulator_[rule.id];
            acc += ctx.deltaTime;
            if (acc < rule.delay) continue;
            acc = 0.0f;
        }

        // fire
        fired_[rule.id] = true;
        lastFiredFrame_[rule.id] = ctx.currentFrame;
        if (rule.cooldown > 0.0f)
            cooldownRemaining_[rule.id] = rule.cooldown;

        TriggerEvent ev;
        ev.type = tc.type;
        ev.ruleId = rule.id;
        ev.sourceLayerId = tc.sourceLayerId;
        ev.targetLayerId = tc.targetLayerId;
        ev.frame = ctx.currentFrame;
        ev.deltaTime = ctx.deltaTime;
        results.push_back(ev);
    }

    // save prev-frame state
    for (const auto& rule : rules_) {
        const auto& tc = rule.trigger;
        if (!tc.sourceLayerId.isEmpty() && ctx.layerIsActive)
            prevActive_[tc.sourceLayerId] = ctx.layerIsActive(tc.sourceLayerId);
    }
    prevFrame_ = ctx.currentFrame;

    return results;
}

} // namespace ArtifactCore
