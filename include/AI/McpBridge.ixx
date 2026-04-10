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
            QJsonObject entry;
            entry[QStringLiteral("name")] =
                QStringLiteral("%1.%2")
                    .arg(tool.value(QStringLiteral("component")).toString(),
                         tool.value(QStringLiteral("method")).toString());
            entry[QStringLiteral("description")] = tool.value(QStringLiteral("description")).toString();
            entry[QStringLiteral("returnType")] = tool.value(QStringLiteral("returnType")).toString();
            entry[QStringLiteral("parameters")] = tool.value(QStringLiteral("parameters")).toArray();
            tools.append(entry);
        }
        }

        QJsonObject capabilities;
        capabilities[QStringLiteral("tools")] = tools;
        return capabilities;
    }

    static QJsonObject handleRequest(const QJsonObject& request, const AIContext& context = AIContext())
    {
        const QString method = request.value(QStringLiteral("method")).toString();
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

            if (!toolCall.contains(QStringLiteral("class")) ||
                !toolCall.contains(QStringLiteral("method"))) {
                return makeError(-32602, QStringLiteral("Invalid tool call payload"));
            }

            const ToolBridgeResult bridgeResult = ToolBridge::executeToolCall(toolCall);
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

        return makeError(-32601, QStringLiteral("Method not found: %1").arg(method));
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
