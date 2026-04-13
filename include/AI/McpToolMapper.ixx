module;
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QString>

export module Core.AI.McpToolMapper;

import std;
import Core.AI.ToolBridge;

export namespace ArtifactCore {

struct McpToolMapping {
  QString mcpToolName;
  QString artifactClassName;
  QString artifactMethodName;
  QJsonObject parameterMapping; // MCP param name -> Artifact param name
};

class McpToolMapper {
public:
  static McpToolMapper &instance() {
    static McpToolMapper mapper;
    return mapper;
  }

  void registerMapping(const QString &mcpToolName,
                       const QString &artifactClassName,
                       const QString &artifactMethodName,
                       const QJsonObject &parameterMapping = {}) {
    McpToolMapping mapping;
    mapping.mcpToolName = mcpToolName;
    mapping.artifactClassName = artifactClassName;
    mapping.artifactMethodName = artifactMethodName;
    mapping.parameterMapping = parameterMapping;
    mappings_[mcpToolName] = mapping;
  }

  bool hasMapping(const QString &mcpToolName) const {
    return mappings_.contains(mcpToolName);
  }

  const McpToolMapping *getMapping(const QString &mcpToolName) const {
    auto it = mappings_.find(mcpToolName);
    if (it != mappings_.end()) {
      return &it.value();
    }
    return nullptr;
  }

  QStringList mappedMcpToolNames() const { return mappings_.keys(); }

  // Convert Artifact internal tool call to MCP format
  QJsonObject
  convertArtifactToolCallToMcp(const QJsonObject &artifactToolCall) const {
    const QString artifactClass = artifactToolCall.value("class").toString();
    const QString artifactMethod = artifactToolCall.value("method").toString();

    // Find MCP tool name for this Artifact class/method
    for (auto it = mappings_.begin(); it != mappings_.end(); ++it) {
      if (it.value().artifactClassName == artifactClass &&
          it.value().artifactMethodName == artifactMethod) {
        QJsonObject mcpToolCall;
        mcpToolCall["name"] = it.key();

        // Map arguments back
        QJsonObject artifactArgs =
            artifactToolCall.value("arguments").toObject();
        QJsonObject mcpArgs;
        for (auto argIt = artifactArgs.begin(); argIt != artifactArgs.end();
             ++argIt) {
          QString mcpParamName = argIt.key();
          // Reverse mapping if needed
          if (it.value().parameterMapping.contains(argIt.key())) {
            mcpParamName =
                it.value().parameterMapping.value(argIt.key()).toString();
          }
          mcpArgs[mcpParamName] = argIt.value();
        }
        mcpToolCall["arguments"] = mcpArgs;
        return mcpToolCall;
      }
    }
    return QJsonObject(); // No mapping found
  }

  // Convert MCP tool call to Artifact internal format
  QJsonObject
  convertMcpToolCallToArtifact(const QJsonObject &mcpToolCall) const {
    const QString mcpToolName = mcpToolCall.value("name").toString();
    const auto mapping = getMapping(mcpToolName);
    if (!mapping) {
      return QJsonObject();
    }

    QJsonObject artifactCall;
    artifactCall["class"] = mapping->artifactClassName;
    artifactCall["method"] = mapping->artifactMethodName;

    // Map parameters
    QJsonObject mcpArgs = mcpToolCall.value("arguments").toObject();
    QJsonObject artifactArgs;
    for (auto it = mcpArgs.begin(); it != mcpArgs.end(); ++it) {
      QString artifactParamName = it.key();
      if (mapping->parameterMapping.contains(it.key())) {
        artifactParamName =
            mapping->parameterMapping.value(it.key()).toString();
      }
      artifactArgs[artifactParamName] = it.value();
    }
    artifactCall["arguments"] = artifactArgs;

    return artifactCall;
  }

private:
  QMap<QString, McpToolMapping> mappings_;

  McpToolMapper() {
    // Register default mappings
    registerDefaultMappings();
  }

  void registerDefaultMappings() {
    // WorkspaceAutomation mappings
    registerMapping("workspace_snapshot", "WorkspaceAutomation",
                    "workspaceSnapshot");
    registerMapping("project_snapshot", "WorkspaceAutomation",
                    "projectSnapshot");
    registerMapping("current_composition_snapshot", "WorkspaceAutomation",
                    "currentCompositionSnapshot");
    registerMapping("selection_snapshot", "WorkspaceAutomation",
                    "selectionSnapshot");
    registerMapping("render_queue_snapshot", "WorkspaceAutomation",
                    "renderQueueSnapshot");
    registerMapping("list_compositions", "WorkspaceAutomation",
                    "listCompositions");
    registerMapping("list_project_items", "WorkspaceAutomation",
                    "listProjectItems");
    registerMapping("list_current_composition_layers", "WorkspaceAutomation",
                    "listCurrentCompositionLayers");
    registerMapping("list_render_queue_jobs", "WorkspaceAutomation",
                    "listRenderQueueJobs");
    registerMapping("create_project", "WorkspaceAutomation", "createProject",
                    QJsonObject{{"name", "projectName"}});
    registerMapping(
        "create_composition", "WorkspaceAutomation", "createComposition",
        QJsonObject{
            {"name", "name"}, {"width", "width"}, {"height", "height"}});
    registerMapping("change_current_composition", "WorkspaceAutomation",
                    "changeCurrentComposition",
                    QJsonObject{{"id", "compositionId"}});

    // CommandSandbox mappings
    registerMapping("run_command", "CommandSandbox", "runCommand",
                    QJsonObject{{"command", "program"},
                                {"args", "arguments"},
                                {"timeout", "timeoutMs"}});

    // MaterialAutomation mappings
    registerMapping(
        "create_material", "MaterialAutomation", "createMaterial",
        QJsonObject{{"name", "name"}, {"properties", "properties"}});
    registerMapping("get_material_properties", "MaterialAutomation",
                    "getMaterialProperties");
    registerMapping("update_material_property", "MaterialAutomation",
                    "updateMaterialProperty",
                    QJsonObject{{"name", "name"},
                                {"property", "property"},
                                {"value", "value"}});
    registerMapping("apply_material_preset", "MaterialAutomation",
                    "applyMaterialPreset");
    registerMapping("list_material_presets", "MaterialAutomation",
                    "listMaterialPresets");
    registerMapping("assign_material_to_layer", "MaterialAutomation",
                    "assignMaterialToLayer");

    // RenderAutomation mappings
    registerMapping("set_camera_position", "RenderAutomation",
                    "setCameraPosition",
                    QJsonObject{{"x", "x"}, {"y", "y"}, {"z", "z"}});
    registerMapping("set_camera_rotation", "RenderAutomation",
                    "setCameraRotation",
                    QJsonObject{{"rx", "rx"}, {"ry", "ry"}, {"rz", "rz"}});
    registerMapping("set_render_mode", "RenderAutomation", "setRenderMode");
    registerMapping("toggle_wireframe", "RenderAutomation", "toggleWireframe");
    registerMapping("set_viewport_background", "RenderAutomation",
                    "setViewportBackground");
    registerMapping("focus_on_layer", "RenderAutomation", "focusOnLayer");

    // FileAutomation mappings
    registerMapping("read_text_file", "FileAutomation", "readTextFile");
    registerMapping(
        "write_text_file", "FileAutomation", "writeTextFile",
        QJsonObject{{"filePath", "filePath"}, {"content", "content"}});
    registerMapping("list_directory", "FileAutomation", "listDirectory");
    registerMapping("file_exists", "FileAutomation", "fileExists");
    registerMapping("create_directory", "FileAutomation", "createDirectory");
    registerMapping("delete_file", "FileAutomation", "deleteFile");
    registerMapping("get_file_info", "FileAutomation", "getFileInfo");
  }

  // ── Auto-generate mappings from ToolBridge schema ──

  // Generate an MCP-friendly tool name from a class.method pair
  static QString generateMcpToolName(const QString& className, const QString& methodName)
  {
    // e.g. "WorkspaceAutomation" + "listCompositions" → "workspace_list_compositions"
    // e.g. "FileAutomation" + "readTextFile" → "file_read_text_file"
    QString classPrefix = className;
    // Strip common suffixes for brevity
    classPrefix.replace(QStringLiteral("Automation"), QString());
    classPrefix.replace(QStringLiteral("Controller"), QString());
    classPrefix.replace(QStringLiteral("Manager"), QString());
    classPrefix.replace(QStringLiteral("Service"), QString());
    classPrefix = classPrefix.trimmed();
    if (classPrefix.isEmpty()) {
        classPrefix = className;
    }

    // Convert camelCase methodName to snake_case
    QString snakeMethod;
    for (int i = 0; i < methodName.size(); ++i) {
        const QChar ch = methodName.at(i);
        if (ch.isUpper() && i > 0) {
            snakeMethod.append(QLatin1Char('_'));
        }
        snakeMethod.append(ch.toLower());
    }

    QString snakeClass;
    for (int i = 0; i < classPrefix.size(); ++i) {
        const QChar ch = classPrefix.at(i);
        if (ch.isUpper() && i > 0) {
            snakeClass.append(QLatin1Char('_'));
        }
        snakeClass.append(ch.toLower());
    }

    // Use last word of class as prefix if it's compound
    const QStringList classParts = snakeClass.split(QLatin1Char('_'));
    const QString shortPrefix = classParts.isEmpty() ? snakeClass : classParts.last();

    return shortPrefix + QLatin1Char('_') + snakeMethod;
  }

  // Build parameter mapping (identity mapping by default)
  static QJsonObject buildIdentityParamMapping(const QJsonArray& parameters)
  {
    QJsonObject mapping;
    for (const auto& val : parameters) {
        const QJsonObject param = val.toObject();
        const QString name = param.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) {
            // Identity: MCP param name == Artifact param name
            mapping[name] = name;
        }
    }
    return mapping;
  }

  // Auto-register all tools from ToolBridge schema
  void autoGenerateFromToolSchema()
  {
    const QString schemaJson = ToolBridge::toolSchemaJson();
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(schemaJson.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonArray tools = doc.object().value(QStringLiteral("tools")).toArray();
    int addedCount = 0;
    for (const auto& val : tools) {
        const QJsonObject tool = val.toObject();
        const QString className = tool.value(QStringLiteral("component")).toString().trimmed();
        const QString methodName = tool.value(QStringLiteral("method")).toString().trimmed();
        if (className.isEmpty() || methodName.isEmpty()) {
            continue;
        }

        const QString mcpName = generateMcpToolName(className, methodName);
        if (hasMapping(mcpName)) {
            continue; // Already registered
        }

        const QJsonArray params = tool.value(QStringLiteral("parameters")).toArray();
        const QJsonObject paramMapping = buildIdentityParamMapping(params);
        registerMapping(mcpName, className, methodName, paramMapping);
        ++addedCount;
    }
  }

  // Sync: clear existing mappings and re-generate from schema
  void syncMappingsFromToolSchema()
  {
    mappings_.clear();
    autoGenerateFromToolSchema();
  }
};

} // namespace ArtifactCore