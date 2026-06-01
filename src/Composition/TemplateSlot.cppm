module;
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringView>

module Composition.TemplateSlot;

import Composition.TemplateSlot;
import Utils.Id;

namespace ArtifactCore {

QJsonObject TemplateSlot::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["displayName"] = displayName;
    obj["targetLayerId"] = targetLayerId;
    obj["defaultValue"] = defaultValue;
    obj["required"] = required;
    obj["isLocked"] = isLocked;
    obj["type"] = static_cast<int>(type);
    return obj;
}

TemplateSlot TemplateSlot::fromJson(const QJsonObject& obj) {
    TemplateSlot slot;
    slot.id = obj["id"].toString();
    slot.displayName = obj["displayName"].toString();
    slot.targetLayerId = obj["targetLayerId"].toString();
    slot.defaultValue = obj["defaultValue"].toString();
    slot.required = obj["required"].toBool();
    slot.isLocked = obj.value("isLocked").toBool();
    slot.type = static_cast<SlotValueType>(obj.value("type").toInt(static_cast<int>(SlotValueType::Text)));
    return slot;
}

QString TemplateVariation::valueForSlot(const QString& slotId) const {
    for (const auto& [sid, value] : slotValues) {
        if (sid == slotId) return value;
    }
    return {};
}

void TemplateVariation::setValueForSlot(const QString& slotId, const QString& value) {
    for (auto& [sid, v] : slotValues) {
        if (sid == slotId) {
            v = value;
            return;
        }
    }
    slotValues.append({slotId, value});
}

QJsonObject TemplateVariation::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["displayName"] = displayName;
    QJsonArray values;
    for (const auto& [slotId, value] : slotValues) {
        QJsonObject entry;
        entry["slotId"] = slotId;
        entry["value"] = value;
        values.append(entry);
    }
    obj["slotValues"] = values;
    return obj;
}

TemplateVariation TemplateVariation::fromJson(const QJsonObject& obj) {
    TemplateVariation variation;
    variation.id = obj["id"].toString();
    variation.displayName = obj["displayName"].toString(obj.value("id").toString());
    const auto values = obj["slotValues"].toArray();
    for (const auto& val : values) {
        if (val.isObject()) {
            const auto entry = val.toObject();
            variation.slotValues.append({entry["slotId"].toString(), entry["value"].toString()});
        }
    }
    return variation;
}

QJsonObject OutputVariant::toJson() const {
    QJsonObject obj;
    obj["width"] = width;
    obj["height"] = height;
    obj["safeAreaPreset"] = safeAreaPreset;
    obj["renderPreset"] = renderPreset;
    obj["namingRule"] = namingRule;
    return obj;
}

OutputVariant OutputVariant::fromJson(const QJsonObject& obj) {
    OutputVariant out;
    out.width = obj.value("width").toInt(1920);
    out.height = obj.value("height").toInt(1080);
    out.safeAreaPreset = obj["safeAreaPreset"].toString();
    out.renderPreset = obj["renderPreset"].toString();
    out.namingRule = obj["namingRule"].toString();
    return out;
}

} // namespace ArtifactCore