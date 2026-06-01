module;
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QMetaType>
#include <functional>

export module Composition.TemplateSlot;

import Utils.Id;

export namespace ArtifactCore {

enum class SlotValueType {
    Text,
    Image,
    Media,
    Color,
    Number
};

struct TemplateSlot {
    SlotValueType type = SlotValueType::Text;
    QString id;
    QString displayName;
    QString targetLayerId;
    QString defaultValue;
    bool required = false;
    bool isLocked = false;

    QJsonObject toJson() const;
    static TemplateSlot fromJson(const QJsonObject& obj);
};

class TemplateVariation {
public:
    QString id;
    QString displayName;
    QVector<std::pair<QString, QString>> slotValues;

    QString valueForSlot(const QString& slotId) const;
    void setValueForSlot(const QString& slotId, const QString& value);
    QJsonObject toJson() const;
    static TemplateVariation fromJson(const QJsonObject& obj);
};

class OutputVariant {
public:
    int width = 1920;
    int height = 1080;
    QString safeAreaPreset;
    QString renderPreset;
    QString namingRule;

    QJsonObject toJson() const;
    static OutputVariant fromJson(const QJsonObject& obj);
};

} // namespace ArtifactCore

Q_DECLARE_METATYPE(ArtifactCore::TemplateSlot)
Q_DECLARE_METATYPE(ArtifactCore::TemplateVariation)
Q_DECLARE_METATYPE(ArtifactCore::OutputVariant)