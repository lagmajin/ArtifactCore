module;
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

module Composition.TemplateLock;

namespace ArtifactCore {

QJsonObject ProtectedRegion::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["displayName"] = displayName;
    obj["scope"] = static_cast<int>(scope);
    obj["editability"] = static_cast<int>(editability);
    obj["description"] = description;
    return obj;
}

ProtectedRegion ProtectedRegion::fromJson(const QJsonObject& obj) {
    ProtectedRegion region;
    region.id = obj["id"].toString();
    region.displayName = obj["displayName"].toString();
    region.scope = static_cast<LockScope>(obj.value("scope").toInt(static_cast<int>(LockScope::Layout)));
    region.editability = static_cast<Editability>(obj.value("editability").toInt(static_cast<int>(Editability::Locked)));
    region.description = obj["description"].toString();
    return region;
}

QJsonObject EditableField::toJson() const {
    QJsonObject obj;
    obj["fieldId"] = fieldId;
    obj["displayName"] = displayName;
    obj["slotId"] = slotId;
    obj["allowedValueType"] = allowedValueType;
    obj["description"] = description;
    return obj;
}

EditableField EditableField::fromJson(const QJsonObject& obj) {
    EditableField field;
    field.fieldId = obj["fieldId"].toString();
    field.displayName = obj["displayName"].toString();
    field.slotId = obj["slotId"].toString();
    field.allowedValueType = obj["allowedValueType"].toString();
    field.description = obj["description"].toString();
    return field;
}

QJsonObject TemplateLockSchema::toJson() const {
    QJsonObject obj;
    obj["templateId"] = templateId;
    obj["templateName"] = templateName;
    obj["isEnabled"] = isEnabled;

    QJsonArray regions;
    for (const auto& r : protectedRegions) {
        regions.append(r.toJson());
    }
    obj["protectedRegions"] = regions;

    QJsonArray fields;
    for (const auto& f : editableFields) {
        fields.append(f.toJson());
    }
    obj["editableFields"] = fields;

    return obj;
}

TemplateLockSchema TemplateLockSchema::fromJson(const QJsonObject& obj) {
    TemplateLockSchema schema;
    schema.templateId = obj["templateId"].toString();
    schema.templateName = obj["templateName"].toString();
    schema.isEnabled = obj.value("isEnabled").toBool(true);

    const auto regions = obj["protectedRegions"].toArray();
    for (const auto& r : regions) {
        if (r.isObject()) {
            schema.protectedRegions.append(ProtectedRegion::fromJson(r.toObject()));
        }
    }

    const auto fields = obj["editableFields"].toArray();
    for (const auto& f : fields) {
        if (f.isObject()) {
            schema.editableFields.append(EditableField::fromJson(f.toObject()));
        }
    }

    return schema;
}

bool TemplateLockSchema::isFieldEditable(const QString& fieldId) const {
    for (const auto& f : editableFields) {
        if (f.fieldId == fieldId) return true;
    }
    for (const auto& r : protectedRegions) {
        if (r.id == fieldId) return r.editability != Editability::Locked;
    }
    return false;
}

bool TemplateLockSchema::isRegionLocked(const QString& regionId) const {
    for (const auto& r : protectedRegions) {
        if (r.id == regionId) return r.editability == Editability::Locked;
    }
    return false;
}

QVector<EditableField> TemplateLockSchema::getEditableFieldsForSlot(const QString& slotId) const {
    QVector<EditableField> result;
    for (const auto& f : editableFields) {
        if (f.slotId == slotId) {
            result.append(f);
        }
    }
    return result;
}

Editability TemplateLockSchema::editabilityForField(const QString& fieldId) const {
    for (const auto& f : editableFields) {
        if (f.fieldId == fieldId) return Editability::Editable;
    }
    for (const auto& r : protectedRegions) {
        if (r.id == fieldId) return r.editability;
    }
    return Editability::Editable;
}

bool TemplateEditGuard::canEditField(const TemplateLockSchema& schema, const QString& fieldId,
                                      QString* outWarning) {
    if (!schema.isEnabled) return true;

    Editability edit = schema.editabilityForField(fieldId);
    if (edit == Editability::Editable) return true;
    if (edit == Editability::EditableWithWarning) {
        if (outWarning) {
            *outWarning = QStringLiteral("Field '%1' is editable but may affect locked regions").arg(fieldId);
        }
        return true;
    }

    if (outWarning) {
        *outWarning = QStringLiteral("Field '%1' is locked by template policy").arg(fieldId);
    }
    return false;
}

bool TemplateEditGuard::canModifyParameter(const TemplateLockSchema& schema,
                                            const QString& paramKey, bool paramOverridable,
                                            QString* outWarning) {
    if (!schema.isEnabled) return paramOverridable;
    if (!paramOverridable) {
        if (outWarning) {
            *outWarning = QStringLiteral("Parameter '%1' is not overridable").arg(paramKey);
        }
        return false;
    }

    if (schema.isRegionLocked(paramKey)) {
        if (outWarning) {
            *outWarning = QStringLiteral("Parameter '%1' is in a locked region").arg(paramKey);
        }
        return false;
    }

    return true;
}

QString TemplateEditGuard::lockReason(const TemplateLockSchema& schema, const QString& fieldId) {
    if (!schema.isEnabled) return {};

    for (const auto& r : schema.protectedRegions) {
        if (r.id == fieldId || r.id == fieldId.section('.', 0, 0)) {
            return QStringLiteral("Protected region: %1 (%2)")
                .arg(r.displayName, r.description);
        }
    }

    return {};
}

QVector<EditableField> TemplateEditGuard::getAccessibleFields(const TemplateLockSchema& schema) {
    if (!schema.isEnabled) return schema.editableFields;

    QVector<EditableField> result;
    for (const auto& f : schema.editableFields) {
        result.append(f);
    }

    for (const auto& r : schema.protectedRegions) {
        if (r.editability != Editability::Locked) {
            EditableField f;
            f.fieldId = r.id;
            f.displayName = r.displayName;
            f.description = r.description;
            result.append(f);
        }
    }

    return result;
}

} // namespace ArtifactCore
