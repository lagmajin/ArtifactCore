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

inline CommandResult commandResultFromVariantMap(const QVariantMap& map)
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
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("create_layer")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("layerType"), QStringLiteral("layerName")}},
                {QStringLiteral("description"), QStringLiteral("Create a new layer (solid/text/null) in the current composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("delete_layer")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId")}},
                {QStringLiteral("description"), QStringLiteral("Remove a layer from the current composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_layer_visible")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("visible")}},
                {QStringLiteral("description"), QStringLiteral("Show or hide a layer")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_layer_blend_mode")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("blendMode")}},
                {QStringLiteral("description"), QStringLiteral("Set the blend mode of a layer")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_layer_opacity")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("opacity")}},
                {QStringLiteral("description"), QStringLiteral("Set the opacity of a layer (0-100)")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_playback_state")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("state")}},
                {QStringLiteral("description"), QStringLiteral("Control playback: play/pause/stop/seek to frame")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("export_composition")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("Add the current composition to the render queue")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("remove_effect")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("effectIndex")}},
                {QStringLiteral("description"), QStringLiteral("Remove an effect from a layer by index")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("get_scene_info")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("Return a full JSON snapshot of the current project, composition, and layers for AI context awareness")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("get_layer_info")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId")}},
                {QStringLiteral("description"), QStringLiteral("Return position/scale/rotation/opacity and effects for a specific layer")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("create_composition")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("name")}},
                {QStringLiteral("description"), QStringLiteral("Create a new composition with optional width/height")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("switch_composition")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("compositionId")}},
                {QStringLiteral("description"), QStringLiteral("Switch the active composition by ID")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("import_asset")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("filePaths[]")}},
                {QStringLiteral("description"), QStringLiteral("Import media files (image/video/audio) into the project")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("duplicate_layer")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId")}},
                {QStringLiteral("description"), QStringLiteral("Duplicate a layer in the current composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("group_layers")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("layerIds[]"), QStringLiteral("groupName")}},
                {QStringLiteral("description"), QStringLiteral("Group multiple layers into a pre-composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_layer_parent")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("parentLayerId")}},
                {QStringLiteral("description"), QStringLiteral("Set parent layer for hierarchical transform")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("split_layer")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId")}},
                {QStringLiteral("description"), QStringLiteral("Split a layer into two at the current playback time")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("get_keyframes")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId")}},
                {QStringLiteral("description"), QStringLiteral("Get all keyframes for a layer's property path")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("delete_keyframe")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("target.propertyPath"), QStringLiteral("frame")}},
                {QStringLiteral("description"), QStringLiteral("Delete a specific keyframe at a given frame number")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_work_area")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("startFrame"), QStringLiteral("endFrame")}},
                {QStringLiteral("description"), QStringLiteral("Set the work area (in/out points) for the active composition")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("add_marker")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("frame"), QStringLiteral("label")}},
                {QStringLiteral("description"), QStringLiteral("Add a timeline marker at a frame with a label")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_effect_parameter")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("effectIndex"), QStringLiteral("paramName"), QStringLiteral("value")}},
                {QStringLiteral("description"), QStringLiteral("Set a parameter value on an effect by layer ID and effect index")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("set_effect_enabled")},
                {QStringLiteral("required"), QVariantList{QStringLiteral("target.layerId"), QStringLiteral("effectIndex"), QStringLiteral("enabled")}},
                {QStringLiteral("description"), QStringLiteral("Enable or disable a specific effect on a layer")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("list_available_effects")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("List all registered effect types that can be added to a layer")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("start_render_queue")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("Start all pending render queue jobs")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("get_render_status")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("Get status and progress of all render queue jobs")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("list_compositions")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("List all compositions in the current project")}
            },
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("list_project_items")},
                {QStringLiteral("required"), QVariantList{}},
                {QStringLiteral("description"), QStringLiteral("List all items (assets, compositions, folders) in the project panel")}
            },
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
               normalized == QStringLiteral("add_effect") ||
               normalized == QStringLiteral("create_layer") ||
               normalized == QStringLiteral("delete_layer") ||
               normalized == QStringLiteral("set_layer_visible") ||
               normalized == QStringLiteral("set_layer_blend_mode") ||
               normalized == QStringLiteral("set_layer_opacity") ||
               normalized == QStringLiteral("set_playback_state") ||
               normalized == QStringLiteral("export_composition") ||
               normalized == QStringLiteral("remove_effect") ||
               normalized == QStringLiteral("get_scene_info") ||
               normalized == QStringLiteral("get_layer_info") ||
               normalized == QStringLiteral("create_composition") ||
               normalized == QStringLiteral("switch_composition") ||
               normalized == QStringLiteral("import_asset") ||
               normalized == QStringLiteral("duplicate_layer") ||
               normalized == QStringLiteral("group_layers") ||
               normalized == QStringLiteral("set_layer_parent") ||
               normalized == QStringLiteral("split_layer") ||
               normalized == QStringLiteral("get_keyframes") ||
               normalized == QStringLiteral("delete_keyframe") ||
               normalized == QStringLiteral("set_work_area") ||
               normalized == QStringLiteral("add_marker") ||
               normalized == QStringLiteral("set_effect_parameter") ||
               normalized == QStringLiteral("set_effect_enabled") ||
               normalized == QStringLiteral("list_available_effects") ||
               normalized == QStringLiteral("start_render_queue") ||
               normalized == QStringLiteral("get_render_status") ||
               normalized == QStringLiteral("list_compositions") ||
               normalized == QStringLiteral("list_project_items");
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
        if (normalized == QStringLiteral("create_layer")) {
            return QStringLiteral("Create Layer");
        }
        if (normalized == QStringLiteral("delete_layer")) {
            return QStringLiteral("Delete Layer");
        }
        if (normalized == QStringLiteral("set_layer_visible")) {
            return QStringLiteral("Set Layer Visibility");
        }
        if (normalized == QStringLiteral("set_layer_blend_mode")) {
            return QStringLiteral("Set Blend Mode");
        }
        if (normalized == QStringLiteral("set_layer_opacity")) {
            return QStringLiteral("Set Layer Opacity");
        }
        if (normalized == QStringLiteral("set_playback_state")) {
            return QStringLiteral("Set Playback State");
        }
        if (normalized == QStringLiteral("export_composition")) {
            return QStringLiteral("Export Composition");
        }
        if (normalized == QStringLiteral("remove_effect")) {
            return QStringLiteral("Remove Effect");
        }
        if (normalized == QStringLiteral("get_scene_info")) {
            return QStringLiteral("Get Scene Info");
        }
        if (normalized == QStringLiteral("get_layer_info")) {
            return QStringLiteral("Get Layer Info");
        }
        if (normalized == QStringLiteral("create_composition")) {
            return QStringLiteral("Create Composition");
        }
        if (normalized == QStringLiteral("switch_composition")) {
            return QStringLiteral("Switch Composition");
        }
        if (normalized == QStringLiteral("import_asset")) {
            return QStringLiteral("Import Asset");
        }
        if (normalized == QStringLiteral("duplicate_layer")) {
            return QStringLiteral("Duplicate Layer");
        }
        if (normalized == QStringLiteral("group_layers")) {
            return QStringLiteral("Group Layers");
        }
        if (normalized == QStringLiteral("set_layer_parent")) {
            return QStringLiteral("Set Layer Parent");
        }
        if (normalized == QStringLiteral("split_layer")) {
            return QStringLiteral("Split Layer");
        }
        if (normalized == QStringLiteral("get_keyframes")) {
            return QStringLiteral("Get Keyframes");
        }
        if (normalized == QStringLiteral("delete_keyframe")) {
            return QStringLiteral("Delete Keyframe");
        }
        if (normalized == QStringLiteral("set_work_area")) {
            return QStringLiteral("Set Work Area");
        }
        if (normalized == QStringLiteral("add_marker")) {
            return QStringLiteral("Add Marker");
        }
        if (normalized == QStringLiteral("set_effect_parameter")) {
            return QStringLiteral("Set Effect Parameter");
        }
        if (normalized == QStringLiteral("set_effect_enabled")) {
            return QStringLiteral("Set Effect Enabled");
        }
        if (normalized == QStringLiteral("list_available_effects")) {
            return QStringLiteral("List Available Effects");
        }
        if (normalized == QStringLiteral("start_render_queue")) {
            return QStringLiteral("Start Render Queue");
        }
        if (normalized == QStringLiteral("get_render_status")) {
            return QStringLiteral("Get Render Status");
        }
        if (normalized == QStringLiteral("list_compositions")) {
            return QStringLiteral("List Compositions");
        }
        if (normalized == QStringLiteral("list_project_items")) {
            return QStringLiteral("List Project Items");
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
