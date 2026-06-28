module;
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

export module Core.AI.CommandIR;

import std;

export namespace ArtifactCore {

struct CommandRequest {
    QString type;
    QVariantMap target;
    QVariantMap arguments;
    QVariantMap metadata;
};

struct CommandResult {
    bool success = false;
    bool valid = false;
    bool executed = false;
    QString type;
    QString error;
    QString undoLabel;
    QVariantMap diagnostics;
    QVariantList details;

    QVariantMap toVariantMap() const
    {
        QVariantMap result;
        result.insert(QStringLiteral("success"), success);
        result.insert(QStringLiteral("valid"), valid);
        result.insert(QStringLiteral("executed"), executed);
        result.insert(QStringLiteral("type"), type);
        result.insert(QStringLiteral("error"), error);
        result.insert(QStringLiteral("undoLabel"), undoLabel);
        result.insert(QStringLiteral("diagnostics"), diagnostics);
        result.insert(QStringLiteral("details"), details);
        return result;
    }
};

static inline CommandResult commandResultFromVariantMap(const QVariantMap& map)
{
    CommandResult result;
    result.success = map.value(QStringLiteral("success")).toBool();
    result.valid = map.value(QStringLiteral("valid")).toBool();
    result.executed = map.value(QStringLiteral("executed")).toBool();
    result.type = map.value(QStringLiteral("type")).toString();
    result.error = map.value(QStringLiteral("error")).toString();
    result.undoLabel = map.value(QStringLiteral("undoLabel")).toString();
    result.diagnostics = map.value(QStringLiteral("diagnostics")).toMap();
    result.details = map.value(QStringLiteral("details")).toList();
    return result;
}

class CommandExecutor {
public:
    virtual ~CommandExecutor() = default;
    virtual CommandResult validate(const CommandRequest& request) const = 0;
    virtual CommandResult execute(const CommandRequest& request) const = 0;
};

struct CommandField {
    QString name;
    bool required = false;
};

struct CommandSpec {
    QString type;
    QVariantList requiredFields;
    QString description;
};

class CommandIR {
public:
    static CommandRequest fromVariantMap(const QVariantMap& command)
    {
        CommandRequest request;
        request.type = command.value(QStringLiteral("type")).toString().trimmed();
        request.target = command.value(QStringLiteral("target")).toMap();
        request.arguments = command;
        request.metadata = command.value(QStringLiteral("metadata")).toMap();
        return request;
    }

    static QVariantList supportedCommands()
    {
        return {
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_property")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("target.propertyPath"), QStringLiteral("value")}},
                {QStringLiteral("description"), QStringLiteral("Set a single supported property path on a layer")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_keyframes")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("target.propertyPath"), QStringLiteral("keys[]")}},
                {QStringLiteral("description"), QStringLiteral("Set multiple keyframes for one property path")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("batch_set_keyframes")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("batches[]")}},
                {QStringLiteral("description"), QStringLiteral("Set keyframes across multiple property paths in one transaction")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("move_layer")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("newIndex")}},
                {QStringLiteral("description"), QStringLiteral("Move a layer within the current composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("rename_layer")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("newName")}},
                {QStringLiteral("description"), QStringLiteral("Rename a layer in the current composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("add_effect")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("effectType")}},
                {QStringLiteral("description"), QStringLiteral("Add an effect to a layer")}
            }
        };
    }

    static bool isSupportedType(const QString& type)
    {
        const QString normalized = type.trimmed();
        if (normalized.isEmpty()) {
            return false;
        }
        return normalized == QStringLiteral("set_property") ||
               normalized == QStringLiteral("set_keyframes") ||
               normalized == QStringLiteral("batch_set_keyframes") ||
               normalized == QStringLiteral("move_layer") ||
               normalized == QStringLiteral("rename_layer") ||
               normalized == QStringLiteral("add_effect");
    }

    static QVariantList requiredFieldsFor(const QString& type)
    {
        const QString normalized = type.trimmed();
        for (const QVariant& item : supportedCommands()) {
            const QVariantMap command = item.toMap();
            if (command.value(QStringLiteral("type")).toString() == normalized) {
                return command.value(QStringLiteral("required")).toList();
            }
        }
        return {};
    }

    static QString describeType(const QString& type)
    {
        const QString normalized = type.trimmed();
        for (const QVariant& item : supportedCommands()) {
            const QVariantMap command = item.toMap();
            if (command.value(QStringLiteral("type")).toString() == normalized) {
                return command.value(QStringLiteral("description")).toString();
            }
        }
        return {};
    }

    static QString undoLabelForType(const QString& type)
    {
        const QString normalized = type.trimmed();
        if (normalized == QStringLiteral("set_property")) {
            return QStringLiteral("Set Property");
        }
        if (normalized == QStringLiteral("set_keyframes")) {
            return QStringLiteral("Set Keyframes");
        }
        if (normalized == QStringLiteral("batch_set_keyframes")) {
            return QStringLiteral("Batch Set Keyframes");
        }
        if (normalized == QStringLiteral("move_layer")) {
            return QStringLiteral("Move Layer");
        }
        if (normalized == QStringLiteral("rename_layer")) {
            return QStringLiteral("Rename Layer");
        }
        if (normalized == QStringLiteral("add_effect")) {
            return QStringLiteral("Add Effect");
        }
        return QStringLiteral("Command");
    }

    static CommandResult validate(const CommandRequest& request)
    {
        CommandResult result;
        result.type = request.type;
        result.undoLabel = undoLabelForType(request.type);
        if (request.type.isEmpty()) {
            result.error = QStringLiteral("Missing command.type");
            return result;
        }
        if (!isSupportedType(request.type)) {
            result.error = QStringLiteral("Unsupported command type");
            return result;
        }

        const QVariantList required = requiredFieldsFor(request.type);
        bool ok = true;
        for (const QVariant& fieldValue : required) {
            const QString field = fieldValue.toString();
            if (field == QStringLiteral("target.layerId")) {
                if (request.target.value(QStringLiteral("layerId")).toString().trimmed().isEmpty()) {
                    ok = false;
                    result.diagnostics.insert(field, QStringLiteral("missing"));
                }
            } else if (field == QStringLiteral("target.propertyPath")) {
                if (request.target.value(QStringLiteral("propertyPath")).toString().trimmed().isEmpty()) {
                    ok = false;
                    result.diagnostics.insert(field, QStringLiteral("missing"));
                }
            } else if (field == QStringLiteral("keys[]")) {
                const QVariantList keys = request.arguments.value(QStringLiteral("keys")).toList();
                if (keys.isEmpty()) {
                    ok = false;
                    result.diagnostics.insert(field, QStringLiteral("missing"));
                }
            } else if (field == QStringLiteral("batches[]")) {
                const QVariantList batches = request.arguments.value(QStringLiteral("batches")).toList();
                if (batches.isEmpty()) {
                    ok = false;
                    result.diagnostics.insert(field, QStringLiteral("missing"));
                }
            } else if (!request.arguments.contains(field)) {
                ok = false;
                result.diagnostics.insert(field, QStringLiteral("missing"));
            }
        }

        result.valid = ok;
        result.success = ok;
        if (!ok && result.error.isEmpty()) {
            result.error = QStringLiteral("Command validation failed");
        }
        return result;
    }
};

} // namespace ArtifactCore
