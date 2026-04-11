module;
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QStringView>
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
    static QString normalizeQuoteCharacters(QString text)
    {
        text.replace(QChar(0x201C), QLatin1Char('"'));
        text.replace(QChar(0x201D), QLatin1Char('"'));
        text.replace(QChar(0x2018), QLatin1Char('\''));
        text.replace(QChar(0x2019), QLatin1Char('\''));
        text.replace(QChar(0xFF02), QLatin1Char('"'));
        text.replace(QChar(0xFF07), QLatin1Char('\''));
        return text;
    }

    static QString stripToolCallEnvelope(QString text)
    {
        QString trimmed = normalizeQuoteCharacters(text).trimmed();
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

        const int toolCallBegin = trimmed.indexOf(QStringLiteral("[TOOL_CALL]"),
                                                  0, Qt::CaseInsensitive);
        if (toolCallBegin >= 0) {
            trimmed = trimmed.mid(toolCallBegin + QStringLiteral("[TOOL_CALL]").size());
            const int toolCallEnd =
                trimmed.indexOf(QStringLiteral("[/TOOL_CALL]"), 0, Qt::CaseInsensitive);
            if (toolCallEnd >= 0) {
                trimmed = trimmed.left(toolCallEnd);
            }
            return trimmed.trimmed();
        }

        return trimmed;
    }

    static QString extractBalancedBlock(const QString& text, int openIndex)
    {
        if (openIndex < 0 || openIndex >= text.size()) {
            return {};
        }

        const QChar openCh = text.at(openIndex);
        const QChar closeCh = (openCh == QLatin1Char('{')) ? QLatin1Char('}')
                                                          : QLatin1Char(']');
        if (openCh != QLatin1Char('{') && openCh != QLatin1Char('[')) {
            return {};
        }

        int depth = 0;
        bool inString = false;
        QChar stringQuote;
        for (int i = openIndex; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (inString) {
                if (ch == QLatin1Char('\\')) {
                    ++i;
                    continue;
                }
                if (ch == stringQuote) {
                    inString = false;
                }
                continue;
            }

            if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
                inString = true;
                stringQuote = ch;
                continue;
            }

            if (ch == openCh) {
                ++depth;
            } else if (ch == closeCh) {
                --depth;
                if (depth == 0) {
                    return text.mid(openIndex, i - openIndex + 1);
                }
            }
        }
        return {};
    }

    static QJsonValue looseTokenToJsonValue(const QString& token)
    {
        QString value = token.trimmed();
        if (value.isEmpty()) {
            return QJsonValue(QString());
        }
        if ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')) &&
             value.size() >= 2) ||
            (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')) &&
             value.size() >= 2)) {
            value = value.mid(1, value.size() - 2);
        }
        value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        value.replace(QStringLiteral("\\'"), QStringLiteral("'"));
        value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));

        if (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (value.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
            return false;
        }
        if (value.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0) {
            return QJsonValue(QJsonValue::Null);
        }

        bool okInt = false;
        const qlonglong intValue = value.toLongLong(&okInt);
        if (okInt) {
            return QJsonValue(static_cast<double>(intValue));
        }

        bool okDouble = false;
        const double doubleValue = value.toDouble(&okDouble);
        if (okDouble) {
            return QJsonValue(doubleValue);
        }

        return value;
    }

    static QJsonValue parseLooseArgumentsValue(const QString& rawArguments)
    {
        QString trimmed = rawArguments.trimmed();
        if (trimmed.isEmpty()) {
            return QJsonObject{};
        }

        if (trimmed.startsWith(QLatin1Char('{')) ||
            trimmed.startsWith(QLatin1Char('['))) {
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError) {
                if (doc.isObject()) {
                    return doc.object();
                }
                if (doc.isArray()) {
                    return doc.array();
                }
            }

            const QString inner =
                trimmed.size() >= 2 ? trimmed.mid(1, trimmed.size() - 2).trimmed()
                                    : QString();
            return parseLooseArgumentsValue(inner);
        }

        QJsonObject object;
        const QRegularExpression pairPattern(
            QStringLiteral("(?:^|[\\s,;{\\n\\r\\t]+)--([A-Za-z0-9_.-]+)(?:\\s+(?:\"((?:[^\"\\\\]|\\\\.)*)\"|'((?:[^'\\\\]|\\\\.)*)'|([^\\s,;]+)))?)"));
        QRegularExpressionMatchIterator it = pairPattern.globalMatch(trimmed);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString key = match.captured(1).trimmed();
            QString value;
            if (match.capturedLength(2) > 0) {
                value = match.captured(2);
            } else if (match.capturedLength(3) > 0) {
                value = match.captured(3);
            } else if (match.capturedLength(4) > 0) {
                value = match.captured(4);
            } else {
                object.insert(key, true);
                continue;
            }
            object.insert(key, looseTokenToJsonValue(value));
        }
        return object;
    }

    static QString extractLooseFieldValue(const QString& text,
                                          const QStringList& fieldNames)
    {
        for (const QString& fieldName : fieldNames) {
            const QString escaped = QRegularExpression::escape(fieldName);
            const QRegularExpression keyPattern(
                QStringLiteral("(?:\"%1\"|'%1'|%1)\\s*(?:=>|:|=)\\s*")
                    .arg(escaped));
            QRegularExpressionMatch match = keyPattern.match(text);
            if (!match.hasMatch()) {
                const QRegularExpression fallbackPattern(
                    QStringLiteral("%1\\s*(?:=>|:|=)\\s*").arg(escaped));
                match = fallbackPattern.match(text);
                if (!match.hasMatch()) {
                    continue;
                }
            }

            int start = match.capturedEnd();
            while (start < text.size() && text.at(start).isSpace()) {
                ++start;
            }
            if (start >= text.size()) {
                continue;
            }

            if (text.at(start) == QLatin1Char('{') || text.at(start) == QLatin1Char('[')) {
                const QString balanced = extractBalancedBlock(text, start);
                if (!balanced.isEmpty()) {
                    return balanced.trimmed();
                }
            }

            int end = start;
            bool inString = false;
            QChar quoteChar;
            while (end < text.size()) {
                const QChar ch = text.at(end);
                if (inString) {
                    if (ch == QLatin1Char('\\')) {
                        end += 2;
                        continue;
                    }
                    if (ch == quoteChar) {
                        inString = false;
                    }
                    ++end;
                    continue;
                }

                if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
                    inString = true;
                    quoteChar = ch;
                    ++end;
                    continue;
                }
                if (ch == QLatin1Char(',') || ch == QLatin1Char('\n') ||
                    ch == QLatin1Char('\r') || ch == QLatin1Char('}') ||
                    ch == QLatin1Char(']')) {
                    break;
                }
                ++end;
            }
            const QString value = text.mid(start, end - start).trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
        return {};
    }

    static bool normalizeLooseToolCallText(const QString& responseText,
                                           QJsonObject* toolCallOut)
    {
        if (!toolCallOut) {
            return false;
        }

        const QString trimmed = stripToolCallEnvelope(responseText);
        QString candidate = extractFirstJsonObjectCandidate(trimmed);
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(candidate.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            const QRegularExpression objectPattern(
                QStringLiteral(R"(\{[\s\S]*\})"));
            const QRegularExpressionMatch objectMatch = objectPattern.match(trimmed);
            if (objectMatch.hasMatch()) {
                candidate = objectMatch.captured(0);
                doc = QJsonDocument::fromJson(candidate.toUtf8(), &parseError);
            }
        }

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            *toolCallOut = doc.object();
            return true;
        }

        const QRegularExpression toolPattern(
            QStringLiteral("(?:\"class\"|\"component\"|\"tool\")\\s*:\\s*\"([^\"]+)\""));
        const QRegularExpression methodPattern(
            QStringLiteral("\"method\"\\s*:\\s*\"([^\"]+)\""));

        const QRegularExpressionMatch toolMatch = toolPattern.match(trimmed);
        const QRegularExpressionMatch methodMatch = methodPattern.match(trimmed);
        QString className = toolMatch.captured(1).trimmed();
        QString methodName = methodMatch.captured(1).trimmed();
        if (methodName.isEmpty() && className.contains(QLatin1Char('.'))) {
            const int splitIndex = className.lastIndexOf(QLatin1Char('.'));
            if (splitIndex > 0 && splitIndex < className.size() - 1) {
                methodName = className.mid(splitIndex + 1).trimmed();
                className = className.left(splitIndex).trimmed();
            }
        }
        if (className.isEmpty() || methodName.isEmpty()) {
            return false;
        }

        QJsonObject normalized;
        normalized[QStringLiteral("class")] = className.trimmed();
        normalized[QStringLiteral("method")] = methodName.trimmed();

        const QString rawArgs = extractLooseFieldValue(
            trimmed, {QStringLiteral("arguments"), QStringLiteral("args")});
        if (!rawArgs.isEmpty()) {
            const QJsonValue looseArgs = parseLooseArgumentsValue(rawArgs);
            if (looseArgs.isObject() || looseArgs.isArray()) {
                normalized[QStringLiteral("arguments")] = looseArgs;
            } else {
                normalized[QStringLiteral("arguments")] = QJsonObject{};
            }
        } else {
            normalized[QStringLiteral("arguments")] = QJsonObject{};
        }

        *toolCallOut = normalized;
        return true;
    }

    static QJsonObject unwrapToolCall(const QJsonObject& toolCall)
    {
        QJsonObject normalized = toolCall;
        if (normalized.contains(QStringLiteral("tool")) && normalized.value(QStringLiteral("tool")).isObject()) {
            normalized = normalized.value(QStringLiteral("tool")).toObject();
        } else if (normalized.contains(QStringLiteral("tool_call")) &&
                   normalized.value(QStringLiteral("tool_call")).isObject()) {
            normalized = normalized.value(QStringLiteral("tool_call")).toObject();
        } else if (normalized.contains(QStringLiteral("tool_calls")) &&
                   normalized.value(QStringLiteral("tool_calls")).isArray() &&
                   !normalized.value(QStringLiteral("tool_calls")).toArray().isEmpty()) {
            normalized = normalized.value(QStringLiteral("tool_calls")).toArray().first().toObject();
        }
        return normalized;
    }

    static bool validateToolCall(QJsonObject* toolCallOut, QString* errorOut = nullptr)
    {
        if (!toolCallOut) {
            if (errorOut) {
                *errorOut = QStringLiteral("Missing tool call payload");
            }
            return false;
        }

        QJsonObject normalized = unwrapToolCall(*toolCallOut);
        QString className = normalized.value(QStringLiteral("class")).toString().trimmed();
        if (className.isEmpty()) {
            className = normalized.value(QStringLiteral("tool")).toString().trimmed();
        }
        if (className.isEmpty()) {
            className = normalized.value(QStringLiteral("component")).toString().trimmed();
        }
        const QString methodName = normalized.value(QStringLiteral("method")).toString().trimmed();
        if (className.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Tool call is missing a class name");
            }
            return false;
        }
        if (methodName.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Tool call is missing a method name");
            }
            return false;
        }

        normalized[QStringLiteral("class")] = className;
        normalized.remove(QStringLiteral("tool"));
        normalized.remove(QStringLiteral("component"));
        normalized[QStringLiteral("method")] = methodName;
        QJsonValue argumentsValue = normalized.value(QStringLiteral("arguments"));
        if (argumentsValue.isUndefined()) {
            argumentsValue = normalized.value(QStringLiteral("args"));
        }
        QJsonObject argumentObject;
        bool hasArgumentObject = false;
        if (argumentsValue.isObject()) {
            argumentObject = argumentsValue.toObject();
            hasArgumentObject = true;
        } else if (argumentsValue.isArray()) {
            const QJsonArray argumentArray = argumentsValue.toArray();
            if (argumentArray.size() == 1 && argumentArray.first().isObject()) {
                argumentObject = argumentArray.first().toObject();
                hasArgumentObject = true;
            }
        }

        if (hasArgumentObject) {
            const IDescribable* constObj =
                DescriptionRegistry::instance().getDescribable(QStringView{className});
            if (constObj) {
                const auto methods = constObj->methodDescriptions();
                for (const auto& method : methods) {
                    if (method.name != methodName) {
                        continue;
                    }
                    QJsonArray orderedArgs;
                    for (const QString& paramName : method.parameterNames) {
                        orderedArgs.append(argumentObject.value(paramName));
                    }
                    normalized[QStringLiteral("arguments")] = orderedArgs;
                    normalized.remove(QStringLiteral("args"));
                    *toolCallOut = normalized;
                    return true;
                }
            }
        } else if (!argumentsValue.isUndefined() && !argumentsValue.isNull() && !argumentsValue.isArray()) {
            QJsonArray argumentsArray;
            argumentsArray.append(argumentsValue);
            normalized[QStringLiteral("arguments")] = argumentsArray;
            normalized.remove(QStringLiteral("args"));
            *toolCallOut = normalized;
            return true;
        } else if (!argumentsValue.isArray()) {
            normalized[QStringLiteral("arguments")] = QJsonArray{};
            normalized.remove(QStringLiteral("args"));
        }
        *toolCallOut = normalized;
        return true;
    }

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
            if (className.trimmed().isEmpty() || methodName.trimmed().isEmpty()) {
                continue;
            }
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
        QString trimmed = normalizeQuoteCharacters(text).trimmed();
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
        if (firstBrace >= 0) {
            const QString balanced = extractBalancedBlock(trimmed, firstBrace);
            if (!balanced.isEmpty()) {
                return balanced;
            }
        }

        const int firstBracket = trimmed.indexOf('[');
        if (firstBracket >= 0) {
            const QString balanced = extractBalancedBlock(trimmed, firstBracket);
            if (!balanced.isEmpty()) {
                return balanced;
            }
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

    static bool tryParseToolCall(const QString& responseText,
                                 QJsonObject* toolCallOut,
                                 QString* errorOut = nullptr)
    {
        if (!toolCallOut) {
            if (errorOut) {
                *errorOut = QStringLiteral("Missing tool call output container");
            }
            return false;
        }

        if (normalizeLooseToolCallText(responseText, toolCallOut)) {
            QString validationError;
            if (validateToolCall(toolCallOut, &validationError)) {
                return true;
            }
            if (errorOut) {
                *errorOut = validationError;
            }
            return false;
        }

        if (errorOut) {
            *errorOut = QStringLiteral(
                "Unable to parse tool call. Expected JSON object or a [TOOL_CALL] block.");
        }

        return false;
    }

    static ToolBridgeResult executeToolCall(const QJsonObject& toolCall)
    {
        ToolBridgeResult result;
        QJsonObject normalizedToolCall = toolCall;
        QString validationError;
        if (!validateToolCall(&normalizedToolCall, &validationError)) {
            result.trace = QStringLiteral("Tool execution rejected:\n- error: %1\n")
                               .arg(validationError);
            return result;
        }

        const QString className = normalizedToolCall.value(QStringLiteral("class")).toString();
        const IDescribable* constObj =
            DescriptionRegistry::instance().getDescribable(QStringView{className});
        if (!constObj) {
            result.trace = QStringLiteral("Tool execution rejected:\n- error: Unknown tool '%1'\n")
                               .arg(className);
            return result;
        }

        result.value = AIToolExecutor::instance().execute(normalizedToolCall);
        result.trace = buildToolTraceMessage(normalizedToolCall, result.value);
        result.handled = true;
        return result;
    }
};

} // namespace ArtifactCore
