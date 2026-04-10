module;
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QJsonParseError>

export module Core.AI.ToolBridge;

import std;
import Core.AI.Describable;
import Core.AI.PromptGenerator;
import Core.AI.ToolExecutor;

export namespace ArtifactCore {

struct ToolBridgeResult {
    bool handled = false;
    QString trace;
    QVariant value;
};

class ToolBridge {
public:
    static QString toolSchemaJson()
    {
        return QString::fromUtf8(AIPromptGenerator::generateToolSchemaJson());
    }

    static QString toolSchemaSummary()
    {
        const QByteArray schemaBytes = AIPromptGenerator::generateToolSchemaJson();
        if (schemaBytes.trimmed().isEmpty()) {
            return QStringLiteral("No tool schema available.");
        }

        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(schemaBytes, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return QStringLiteral("Failed to parse tool schema JSON.");
        }

        const QJsonArray tools = doc.object().value(QStringLiteral("tools")).toArray();
        if (tools.isEmpty()) {
            return QStringLiteral("No registered tools.");
        }

        QStringList lines;
        lines << QStringLiteral("Registered tools: %1").arg(tools.size());
        for (const QJsonValue& value : tools) {
            const QJsonObject tool = value.toObject();
            const QString className = tool.value(QStringLiteral("component")).toString();
            const QString methodName = tool.value(QStringLiteral("method")).toString();
            const QString description = tool.value(QStringLiteral("description")).toString();
            const QString returnType = tool.value(QStringLiteral("returnType")).toString();
            QString line = QStringLiteral("- %1.%2").arg(className, methodName);
            if (!returnType.trimmed().isEmpty()) {
                line += QStringLiteral(" -> %1").arg(returnType.trimmed());
            }
            if (!description.trimmed().isEmpty()) {
                line += QStringLiteral(" | %1").arg(description.trimmed());
            }
            lines << line;
        }
        return lines.join(QStringLiteral("\n"));
    }

    static QString extractFirstJsonObjectCandidate(const QString& text)
    {
        QString trimmed = text.trimmed();
        if (trimmed.startsWith(QStringLiteral("```"))) {
            const int firstBreak = trimmed.indexOf('\n');
            if (firstBreak >= 0) {
                trimmed = trimmed.mid(firstBreak + 1);
            }
            if (trimmed.endsWith(QStringLiteral("```"))) {
                trimmed.chop(3);
            }
            trimmed = trimmed.trimmed();
        }

        const int firstBrace = trimmed.indexOf('{');
        const int lastBrace = trimmed.lastIndexOf('}');
        if (firstBrace >= 0 && lastBrace > firstBrace) {
            return trimmed.mid(firstBrace, lastBrace - firstBrace + 1);
        }
        return trimmed;
    }

    static QString variantToCompactJsonString(const QVariant& value)
    {
        if (!value.isValid()) {
            return QStringLiteral("(null)");
        }
        const QJsonValue jsonValue = QJsonValue::fromVariant(value);
        if (jsonValue.isObject()) {
            return QString::fromUtf8(
                QJsonDocument(jsonValue.toObject()).toJson(QJsonDocument::Compact));
        }
        if (jsonValue.isArray()) {
            return QString::fromUtf8(
                QJsonDocument(jsonValue.toArray()).toJson(QJsonDocument::Compact));
        }
        if (jsonValue.isString()) {
            return jsonValue.toString();
        }
        if (jsonValue.isBool()) {
            return jsonValue.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        if (jsonValue.isDouble()) {
            return QString::number(jsonValue.toDouble());
        }
        return value.toString();
    }

    static QString buildToolTraceMessage(const QJsonObject& toolCall, const QVariant& toolResult)
    {
        const QString className = toolCall.value(QStringLiteral("class")).toString();
        const QString methodName = toolCall.value(QStringLiteral("method")).toString();
        const QString argsJson = QString::fromUtf8(
            QJsonDocument(QJsonObject{{QStringLiteral("arguments"),
                                       toolCall.value(QStringLiteral("arguments"))}})
                .toJson(QJsonDocument::Compact));
        return QStringLiteral(
                   "Tool execution result:\n"
                   "- class: %1\n"
                   "- method: %2\n"
                   "- arguments: %3\n"
                   "- result: %4\n")
            .arg(className,
                 methodName,
                 argsJson,
                 variantToCompactJsonString(toolResult));
    }

    static QString buildToolArgumentsTemplate(const QJsonObject& tool)
    {
        const QJsonArray params = tool.value(QStringLiteral("parameters")).toArray();
        if (params.isEmpty()) {
            return QStringLiteral("[]");
        }

        QJsonArray templateArgs;
        for (const QJsonValue& value : params) {
            const QJsonObject param = value.toObject();
            const QString typeName = param.value(QStringLiteral("type")).toString().trimmed();
            const QString paramName = param.value(QStringLiteral("name")).toString().trimmed();
            if (typeName.isEmpty()) {
                templateArgs.append(QJsonValue(QStringLiteral("")));
                continue;
            }
            if (typeName.contains(QStringLiteral("List"), Qt::CaseInsensitive) ||
                typeName.contains(QStringLiteral("Array"), Qt::CaseInsensitive)) {
                templateArgs.append(QJsonArray{});
            } else if (typeName.contains(QStringLiteral("bool"), Qt::CaseInsensitive)) {
                templateArgs.append(false);
            } else if (typeName.contains(QStringLiteral("int"), Qt::CaseInsensitive) ||
                       typeName.contains(QStringLiteral("double"), Qt::CaseInsensitive) ||
                       typeName.contains(QStringLiteral("float"), Qt::CaseInsensitive)) {
                templateArgs.append(0);
            } else {
                const QString placeholder = paramName.isEmpty()
                    ? QStringLiteral("<value>")
                    : QStringLiteral("<%1>").arg(paramName);
                templateArgs.append(placeholder);
            }
        }

        return QString::fromUtf8(
            QJsonDocument(templateArgs).toJson(QJsonDocument::Indented));
    }

    static bool tryParseToolCall(const QString& responseText, QJsonObject* toolCallOut)
    {
        if (!toolCallOut) {
            return false;
        }

        const QString candidate = extractFirstJsonObjectCandidate(responseText);
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(candidate.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return false;
        }

        QJsonObject toolCall = doc.object();
        if (toolCall.contains(QStringLiteral("tool")) && toolCall.value(QStringLiteral("tool")).isObject()) {
            toolCall = toolCall.value(QStringLiteral("tool")).toObject();
        } else if (toolCall.contains(QStringLiteral("tool_call")) &&
                   toolCall.value(QStringLiteral("tool_call")).isObject()) {
            toolCall = toolCall.value(QStringLiteral("tool_call")).toObject();
        } else if (toolCall.contains(QStringLiteral("tool_calls")) &&
                   toolCall.value(QStringLiteral("tool_calls")).isArray() &&
                   !toolCall.value(QStringLiteral("tool_calls")).toArray().isEmpty()) {
            toolCall = toolCall.value(QStringLiteral("tool_calls")).toArray().first().toObject();
        }

        if (!toolCall.contains(QStringLiteral("class")) ||
            !toolCall.contains(QStringLiteral("method"))) {
            return false;
        }

        *toolCallOut = toolCall;
        return true;
    }

    static ToolBridgeResult executeToolCall(const QJsonObject& toolCall)
    {
        ToolBridgeResult result;
        result.value = AIToolExecutor::instance().execute(toolCall);
        result.trace = buildToolTraceMessage(toolCall, result.value);
        result.handled = true;
        return result;
    }
};

} // namespace ArtifactCore
