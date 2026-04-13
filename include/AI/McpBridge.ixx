module;
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>

export module Core.AI.McpBridge;

import std;
import Core.AI.Context;
import Core.AI.ToolBridge;

export namespace ArtifactCore {

struct McpFrame {
    QString jsonrpc = QStringLiteral("2.0");
    QJsonObject payload;
};

struct McpResponse {
    bool ok = false;
    QJsonObject response;
    QString errorText;
};

class McpBridge {
public:
    static QByteArray encodeFrame(const QJsonObject& message)
    {
        const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
        return QByteArrayLiteral("Content-Length: ") +
               QByteArray::number(body.size()) +
               QByteArrayLiteral("\r\n\r\n") +
               body;
    }

    static QList<QJsonObject> decodeFrames(const QByteArray& bytes)
    {
        QList<QJsonObject> messages;
        int offset = 0;
        while (offset < bytes.size()) {
            const int headerEnd = bytes.indexOf("\r\n\r\n", offset);
            if (headerEnd < 0) {
                break;
            }
            const QByteArray header = bytes.mid(offset, headerEnd - offset);
            const QList<QByteArray> headerLines = header.split('\n');
            int contentLength = -1;
            for (QByteArray line : headerLines) {
                line = line.trimmed();
                if (line.toLower().startsWith("content-length:")) {
                    const QByteArray value = line.mid(line.indexOf(':') + 1).trimmed();
                    bool ok = false;
                    contentLength = value.toInt(&ok);
                    if (!ok) {
                        contentLength = -1;
                    }
                }
            }
            if (contentLength < 0) {
                break;
            }
            const int bodyStart = headerEnd + 4;
            if (bodyStart + contentLength > bytes.size()) {
                break;
            }
            const QByteArray body = bytes.mid(bodyStart, contentLength);
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                messages.push_back(doc.object());
            }
            offset = bodyStart + contentLength;
        }
        return messages;
    }

    static bool tryPopFrame(QByteArray* buffer, QJsonObject* message)
    {
        if (!buffer || !message) {
            return false;
        }

        const int headerEnd = buffer->indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return false;
        }

        const QByteArray header = buffer->left(headerEnd);
        const QList<QByteArray> headerLines = header.split('\n');
        int contentLength = -1;
        for (QByteArray line : headerLines) {
            line = line.trimmed();
            if (line.toLower().startsWith("content-length:")) {
                const QByteArray value = line.mid(line.indexOf(':') + 1).trimmed();
                bool ok = false;
                contentLength = value.toInt(&ok);
                if (!ok) {
                    contentLength = -1;
                }
            }
        }
        if (contentLength < 0) {
            return false;
        }

        const int bodyStart = headerEnd + 4;
        if (bodyStart + contentLength > buffer->size()) {
            return false;
        }

        const QByteArray body = buffer->mid(bodyStart, contentLength);
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return false;
        }

        *message = doc.object();
        buffer->remove(0, bodyStart + contentLength);
        return true;
    }

    static QJsonObject capabilityList()
    {
        QJsonArray tools;
        const QJsonDocument schema = QJsonDocument::fromJson(ToolBridge::toolSchemaJson().toUtf8());
        if (schema.isObject()) {
            const QJsonArray schemaTools = schema.object().value(QStringLiteral("tools")).toArray();
            for (const QJsonValue& value : schemaTools) {
                const QJsonObject tool = value.toObject();
                const QString componentName = tool.value(QStringLiteral("component")).toString().trimmed();
                const QString methodName = tool.value(QStringLiteral("method")).toString().trimmed();
                if (componentName.isEmpty() || methodName.isEmpty()) {
                    continue;
                }
                QJsonObject entry;
                entry[QStringLiteral("name")] = componentName + QLatin1Char('.') + methodName;
                entry[QStringLiteral("description")] = tool.value(QStringLiteral("description")).toString();
                entry[QStringLiteral("returnType")] = tool.value(QStringLiteral("returnType")).toString();
                entry[QStringLiteral("parameters")] = tool.value(QStringLiteral("parameters")).toArray();
                tools.append(entry);
            }
        }

        QJsonObject capabilities;
        capabilities[QStringLiteral("tools")] = tools;
        capabilities[QStringLiteral("resources")] = QJsonObject{
            {QStringLiteral("list"), true},
            {QStringLiteral("read"), true}
        };
        capabilities[QStringLiteral("prompts")] = QJsonObject{
            {QStringLiteral("list"), true},
            {QStringLiteral("get"), true}
        };
        return capabilities;
    }

    // --- Resources ---

    static QJsonObject resourceList()
    {
        QJsonArray resources;
        resources.append(QJsonObject{
            {QStringLiteral("uri"), QStringLiteral("project://current")},
            {QStringLiteral("name"), QStringLiteral("Current Project")},
            {QStringLiteral("description"), QStringLiteral("Metadata of the currently open project")},
            {QStringLiteral("mimeType"), QStringLiteral("application/json")}
        });
        resources.append(QJsonObject{
            {QStringLiteral("uri"), QStringLiteral("composition://current")},
            {QStringLiteral("name"), QStringLiteral("Current Composition")},
            {QStringLiteral("description"), QStringLiteral("Settings and layer list of the current composition")},
            {QStringLiteral("mimeType"), QStringLiteral("application/json")}
        });
        resources.append(QJsonObject{
            {QStringLiteral("uri"), QStringLiteral("composition://current/layers")},
            {QStringLiteral("name"), QStringLiteral("Current Composition Layers")},
            {QStringLiteral("description"), QStringLiteral("Detailed layer list of the current composition")},
            {QStringLiteral("mimeType"), QStringLiteral("application/json")}
        });
        resources.append(QJsonObject{
            {QStringLiteral("uri"), QStringLiteral("render-queue://current")},
            {QStringLiteral("name"), QStringLiteral("Render Queue")},
            {QStringLiteral("description"), QStringLiteral("Current render queue jobs and status")},
            {QStringLiteral("mimeType"), QStringLiteral("application/json")}
        });
        resources.append(QJsonObject{
            {QStringLiteral("uri"), QStringLiteral("playback://current")},
            {QStringLiteral("name"), QStringLiteral("Playback State")},
            {QStringLiteral("description"), QStringLiteral("Current playback state (frame, speed, range)")},
            {QStringLiteral("mimeType"), QStringLiteral("application/json")}
        });
        return QJsonObject{{QStringLiteral("resources"), resources}};
    }

    static QJsonObject resourceRead(const QString& uri, const AIContext& context = AIContext())
    {
        auto makeContent = [&](const QString& text, const QString& mimeType = QStringLiteral("application/json")) {
            return QJsonObject{
                {QStringLiteral("contents"), QJsonArray{QJsonObject{
                    {QStringLiteral("uri"), uri},
                    {QStringLiteral("mimeType"), mimeType},
                    {QStringLiteral("text"), text}
                }}}
            };
        };

        if (uri == QStringLiteral("project://current")) {
            const auto proj = context.toJson().value(QStringLiteral("project"));
            return makeContent(proj.isObject() && !proj.toObject().isEmpty()
                ? QJsonDocument(proj.toObject()).toJson(QJsonDocument::Compact)
                : QStringLiteral("{\"error\":\"no project open\"}"));
        }
        if (uri == QStringLiteral("composition://current") || uri == QStringLiteral("composition://current/layers")) {
            const auto comp = context.toJson().value(QStringLiteral("composition"));
            return makeContent(comp.isObject() && !comp.toObject().isEmpty()
                ? QJsonDocument(comp.toObject()).toJson(QJsonDocument::Compact)
                : QStringLiteral("{\"error\":\"no composition open\"}"));
        }
        if (uri == QStringLiteral("render-queue://current")) {
            const auto rq = context.toJson().value(QStringLiteral("renderQueue"));
            return makeContent(rq.isObject() && !rq.toObject().isEmpty()
                ? QJsonDocument(rq.toObject()).toJson(QJsonDocument::Compact)
                : QStringLiteral("{\"renderQueue\":[]}"));
        }
        if (uri == QStringLiteral("playback://current")) {
            const auto pb = context.toJson().value(QStringLiteral("playback"));
            return makeContent(pb.isObject() && !pb.toObject().isEmpty()
                ? QJsonDocument(pb.toObject()).toJson(QJsonDocument::Compact)
                : QStringLiteral("{\"error\":\"no playback context\"}"));
        }
        return QJsonObject{{QStringLiteral("error"), QStringLiteral("Resource not found: ") + uri}};
    }

    // --- Prompts ---

    static QJsonObject promptList()
    {
        QJsonArray prompts;
        prompts.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("compose_new_project")},
            {QStringLiteral("description"), QStringLiteral("Guide for setting up a new project with recommended composition settings")},
            {QStringLiteral("arguments"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("purpose")},
                            {QStringLiteral("description"), QStringLiteral("Purpose of the project (e.g. motion_graphics, vfx_compositing, color_grading)")},
                            {QStringLiteral("required"), false}}
            }}
        });
        prompts.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("add_vfx_to_layer")},
            {QStringLiteral("description"), QStringLiteral("Guide for adding visual effects to a selected layer")},
            {QStringLiteral("arguments"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("effect_type")},
                            {QStringLiteral("description"), QStringLiteral("Type of effect (e.g. blur, glow, color_correction, distortion)")},
                            {QStringLiteral("required"), true}}
            }}
        });
        prompts.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug_render_issue")},
            {QStringLiteral("description"), QStringLiteral("Diagnostic workflow for rendering problems (black frames, artifacts, crashes)")},
            {QStringLiteral("arguments"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("symptom")},
                            {QStringLiteral("description"), QStringLiteral("Description of the issue")},
                            {QStringLiteral("required"), true}}
            }}
        });
        prompts.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("optimize_composition_performance")},
            {QStringLiteral("description"), QStringLiteral("Performance optimization checklist for slow compositions")},
            {QStringLiteral("arguments"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("layer_count")},
                            {QStringLiteral("description"), QStringLiteral("Approximate number of layers")},
                            {QStringLiteral("required"), false}}
            }}
        });
        return QJsonObject{{QStringLiteral("prompts"), prompts}};
    }

    static QJsonObject promptGet(const QString& name)
    {
        const struct PromptEntry {
            const char* name;
            const char* systemPrompt;
            const char* userTemplate;
        } kPrompts[] = {
            {"compose_new_project",
             "You are an expert compositing supervisor. Help the user set up a new project with industry-standard settings.",
             "The user wants to create a project for: {{purpose}}.\n\nRecommend composition size, frame rate, color space, and bit depth. Explain your reasoning briefly."},
            {"add_vfx_to_layer",
             "You are a VFX pipeline specialist. Guide the user through adding effects to a layer in ArtifactStudio.",
             "The user wants to add a {{effect_type}} effect.\n\n1. Tell them which effect class to use.\n2. Show the key parameters.\n3. Give recommended starting values.\n4. Mention any performance considerations."},
            {"debug_render_issue",
             "You are a rendering debugger. Help the user diagnose and fix rendering issues in ArtifactStudio.",
             "The user reports: {{symptom}}.\n\nAsk for the following if not provided:\n- Number of layers\n- Whether GPU blend is enabled\n- Preview quality setting\n- Any recent changes\n\nThen provide a step-by-step diagnostic plan starting from the most likely cause."},
            {"optimize_composition_performance",
             "You are a performance optimization expert for ArtifactStudio compositions.",
             "The composition has approximately {{layer_count}} layers and is running slowly.\n\nProvide a prioritized checklist:\n1. Check GPU readback status\n2. Verify texture cache behavior\n3. Review event dispatch patterns\n4. Suggest preview quality adjustments\n5. Identify layers with CPU rasterizer effects"}
        };

        for (const auto& entry : kPrompts) {
            if (name == QLatin1String(entry.name)) {
                QJsonObject prompt;
                prompt[QStringLiteral("name")] = QLatin1String(entry.name);
                prompt[QStringLiteral("description")] = QLatin1String(entry.systemPrompt);
                prompt[QStringLiteral("messages")] = QJsonArray{
                    QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"), QLatin1String(entry.systemPrompt)}},
                    QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), QLatin1String(entry.userTemplate)}}
                };
                return QJsonObject{{QStringLiteral("prompt"), prompt}};
            }
        }
        return QJsonObject{{QStringLiteral("error"), QStringLiteral("Prompt not found: ") + name}};
    }

    static QJsonObject handleRequest(const QJsonObject& request, const AIContext& context = AIContext())
    {
        const QString method = request.value(QStringLiteral("method")).toString().trimmed();
        const QJsonValue idValue = request.value(QStringLiteral("id"));
        const QJsonObject params = request.value(QStringLiteral("params")).toObject();
        AIContext effectiveContext = context;
        if (params.contains(QStringLiteral("context")) && params.value(QStringLiteral("context")).isObject()) {
            effectiveContext = AIContext::fromJson(params.value(QStringLiteral("context")).toObject());
        }

        auto makeResponse = [&](const QJsonValue& result) {
            QJsonObject response;
            response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            if (!idValue.isUndefined() && !idValue.isNull()) {
                response[QStringLiteral("id")] = idValue;
            }
            response[QStringLiteral("result")] = result;
            return response;
        };

        auto makeError = [&](int code, const QString& message) {
            QJsonObject error;
            error[QStringLiteral("code")] = code;
            error[QStringLiteral("message")] = message;
            QJsonObject response;
            response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            if (!idValue.isUndefined() && !idValue.isNull()) {
                response[QStringLiteral("id")] = idValue;
            }
            response[QStringLiteral("error")] = error;
            return response;
        };

        if (method.isEmpty()) {
            return makeError(-32600, QStringLiteral("Invalid request: missing method"));
        }

        if (method == QStringLiteral("initialize")) {
            QJsonObject result;
            result[QStringLiteral("protocolVersion")] = QStringLiteral("2024-11-05");
            result[QStringLiteral("serverInfo")] = QJsonObject{
                {QStringLiteral("name"), QStringLiteral("ArtifactStudio")},
                {QStringLiteral("version"), QStringLiteral("0.9.0")}
            };
            result[QStringLiteral("capabilities")] = capabilityList();
            result[QStringLiteral("context")] = effectiveContext.toJson();
            return makeResponse(result);
        }

        if (method == QStringLiteral("tools/list")) {
            QJsonObject result;
            result[QStringLiteral("tools")] = capabilityList().value(QStringLiteral("tools")).toArray();
            result[QStringLiteral("context")] = effectiveContext.toJson();
            return makeResponse(result);
        }

        if (method == QStringLiteral("tools/call")) {
            QJsonObject toolCall;
            if (params.contains(QStringLiteral("tool")) && params.value(QStringLiteral("tool")).isObject()) {
                toolCall = params.value(QStringLiteral("tool")).toObject();
            } else {
                toolCall[QStringLiteral("class")] = params.value(QStringLiteral("class")).toString();
                toolCall[QStringLiteral("method")] = params.value(QStringLiteral("method")).toString();
                toolCall[QStringLiteral("arguments")] = params.value(QStringLiteral("arguments")).toArray();
            }

            const ToolBridgeResult bridgeResult = ToolBridge::executeToolCall(toolCall);
            if (!bridgeResult.handled) {
                return makeError(-32602,
                                 bridgeResult.trace.isEmpty()
                                     ? QStringLiteral("Invalid tool call payload")
                                     : bridgeResult.trace);
            }
            QJsonObject result;
            result[QStringLiteral("content")] = bridgeResult.trace;
            result[QStringLiteral("structuredContent")] = QJsonValue::fromVariant(bridgeResult.value);
            result[QStringLiteral("handled")] = bridgeResult.handled;
            result[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            result[QStringLiteral("context")] = effectiveContext.toJson();
            return makeResponse(result);
        }

        if (method == QStringLiteral("ping")) {
            return makeResponse(QJsonObject{
                {QStringLiteral("pong"), true},
                {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
            });
        }

        if (method == QStringLiteral("resources/list")) {
            return makeResponse(resourceList());
        }

        if (method == QStringLiteral("resources/read")) {
            const QString uri = params.value(QStringLiteral("uri")).toString();
            if (uri.isEmpty()) {
                return makeError(-32602, QStringLiteral("Missing required parameter: uri"));
            }
            return makeResponse(resourceRead(uri, effectiveContext));
        }

        if (method == QStringLiteral("prompts/list")) {
            return makeResponse(promptList());
        }

        if (method == QStringLiteral("prompts/get")) {
            const QString name = params.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) {
                return makeError(-32602, QStringLiteral("Missing required parameter: name"));
            }
            return makeResponse(promptGet(name));
        }

        return makeError(-32601, QStringLiteral("Method not found: ") + method);
    }

    static QByteArray handleFrame(const QByteArray& frame, const AIContext& context = AIContext())
    {
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(frame, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return encodeFrame(QJsonObject{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), -32700},
                                                      {QStringLiteral("message"), error.errorString()}}}
            });
        }
        return encodeFrame(handleRequest(doc.object(), context));
    }
};

} // namespace ArtifactCore
