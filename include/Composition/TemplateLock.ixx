module;
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <utility>

export module Composition.TemplateLock;

export namespace ArtifactCore {

export enum class LockScope {
    Layout,
    ExportSettings,
    CompositionStructure,
    LayerProperties,
    Effects,
    All
};

export enum class Editability {
    Locked,
    Editable,
    EditableWithWarning
};

export struct ProtectedRegion {
    QString id;
    QString displayName;
    LockScope scope = LockScope::Layout;
    Editability editability = Editability::Locked;
    QString description;

    QJsonObject toJson() const;
    static ProtectedRegion fromJson(const QJsonObject& obj);
};

export struct EditableField {
    QString fieldId;
    QString displayName;
    QString slotId;
    QString allowedValueType;
    QString description;

    QJsonObject toJson() const;
    static EditableField fromJson(const QJsonObject& obj);
};

export class TemplateLockSchema {
public:
    QString templateId;
    QString templateName;
    QVector<ProtectedRegion> protectedRegions;
    QVector<EditableField> editableFields;
    bool isEnabled = true;

    QJsonObject toJson() const;
    static TemplateLockSchema fromJson(const QJsonObject& obj);

    bool isFieldEditable(const QString& fieldId) const;
    bool isRegionLocked(const QString& regionId) const;
    QVector<EditableField> getEditableFieldsForSlot(const QString& slotId) const;
    Editability editabilityForField(const QString& fieldId) const;
};

export class TemplateEditGuard {
public:
    static bool canEditField(const TemplateLockSchema& schema, const QString& fieldId,
                             QString* outWarning = nullptr);

    static bool canModifyParameter(const TemplateLockSchema& schema,
                                    const QString& paramKey, bool paramOverridable,
                                    QString* outWarning = nullptr);

    static QString lockReason(const TemplateLockSchema& schema, const QString& fieldId);

    static QVector<EditableField> getAccessibleFields(const TemplateLockSchema& schema);
};

} // namespace ArtifactCore
