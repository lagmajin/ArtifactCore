module;
#include <algorithm>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>
#include <QHash>
#include <QVector>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSet>

export module Core.AI.McpBridge;

import std;
import Core.AI.Context;
import Core.AI.ToolBridge;
import Diagnostics.Logger;
import Core.Diagnostics.Trace;
import Core.Diagnostics.DebugIdentity;
import Property;
import Script.Expression.Evaluator;

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
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.log")},
            {QStringLiteral("description"), QStringLiteral("直近のアプリケーションログを取得")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("count")},
                             {QStringLiteral("type"), QStringLiteral("int")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.logCategory")},
            {QStringLiteral("description"), QStringLiteral("カテゴリ指定でアプリケーションログを取得")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("category")},
                             {QStringLiteral("type"), QStringLiteral("string")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("count")},
                             {QStringLiteral("type"), QStringLiteral("int")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.filterLog")},
            {QStringLiteral("description"), QStringLiteral("検索文字列でログを絞り込む")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("query")},
                             {QStringLiteral("type"), QStringLiteral("string")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("count")},
                             {QStringLiteral("type"), QStringLiteral("int")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.getTools")},
            {QStringLiteral("description"), QStringLiteral("利用可能なMCPツール一覧を取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        for (const QString& name : {QStringLiteral("debug.state"),
                                    QStringLiteral("debug.pause"),
                                    QStringLiteral("debug.resume"),
                                    QStringLiteral("debug.getSelection"),
                                    QStringLiteral("debug.getViewport"),
                                    QStringLiteral("debug.compositionState"),
                                    QStringLiteral("debug.layerState"),
                                    QStringLiteral("debug.listCompositions"),
                                    QStringLiteral("debug.listLayers"),
                                    QStringLiteral("debug.renderQueue"),
                                    QStringLiteral("debug.getFarm"),
                                    QStringLiteral("debug.stepForward"),
                                    QStringLiteral("debug.stepToFrame"),
                                    QStringLiteral("debug.getColorPipeline"),
                                    QStringLiteral("debug.getMaskPaths"),
                                    QStringLiteral("debug.gpuMemory"),
                                    QStringLiteral("debug.getRig")}) {
            tools.append(QJsonObject{
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), name == QStringLiteral("debug.state")
                    ? QStringLiteral("現在のMCPデバッグ状態を取得")
                    : name == QStringLiteral("debug.getSelection") ? QStringLiteral("現在の選択状態を取得")
                    : name == QStringLiteral("debug.getViewport") ? QStringLiteral("現在のviewport状態を取得")
                    : name == QStringLiteral("debug.compositionState") ? QStringLiteral("現在のコンポジション状態を取得")
                    : name == QStringLiteral("debug.layerState") ? QStringLiteral("選択レイヤーの状態を取得")
                    : name == QStringLiteral("debug.listCompositions") ? QStringLiteral("workspaceのコンポジション一覧を取得")
                    : name == QStringLiteral("debug.listLayers") ? QStringLiteral("現在コンポジションのレイヤー一覧を取得")
                    : name == QStringLiteral("debug.renderQueue") ? QStringLiteral("現在のレンダーキュー状態を取得")
                    : name == QStringLiteral("debug.getFarm") ? QStringLiteral("render farm状態を取得")
                    : name == QStringLiteral("debug.stepForward") ? QStringLiteral("Playbackを指定frame数進める")
                    : name == QStringLiteral("debug.stepToFrame") ? QStringLiteral("Playbackを指定frameへ移動")
                    : name == QStringLiteral("debug.getColorPipeline") ? QStringLiteral("カラーパイプライン状態を取得")
                    : name == QStringLiteral("debug.getMaskPaths") ? QStringLiteral("選択レイヤーのマスクパスを取得")
                    : name == QStringLiteral("debug.gpuMemory") ? QStringLiteral("GPUメモリとテクスチャキャッシュ状態を取得")
                    : name == QStringLiteral("debug.getRig") ? QStringLiteral("Rig2D状態を取得")
                    : QStringLiteral("MCPデバッグセッションを一時停止または再開")},
                {QStringLiteral("parameters"), name == QStringLiteral("debug.stepForward")
                    ? QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("frames")},
                                             {QStringLiteral("type"), QStringLiteral("int")}}}
                    : name == QStringLiteral("debug.stepToFrame")
                        ? QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("frame")},
                                                 {QStringLiteral("type"), QStringLiteral("int")}}}
                        : QJsonArray{}}
            });
        }
        for (const QString& name : {QStringLiteral("debug.addDataBreakpoint"),
                                    QStringLiteral("debug.removeDataBreakpoint"),
                                    QStringLiteral("debug.listDataBreakpoints")}) {
            tools.append(QJsonObject{
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), name.endsWith(QStringLiteral("addDataBreakpoint"))
                    ? QStringLiteral("MCPデバッグ条件を追加")
                    : name.endsWith(QStringLiteral("removeDataBreakpoint"))
                        ? QStringLiteral("MCPデバッグ条件を削除")
                        : QStringLiteral("MCPデバッグ条件を一覧表示")},
                {QStringLiteral("parameters"), QJsonArray{
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("kind")},
                                 {QStringLiteral("type"), QStringLiteral("string")}},
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("value")},
                                 {QStringLiteral("type"), QStringLiteral("variant")}},
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("id")},
                                 {QStringLiteral("type"), QStringLiteral("int")}}
                }}
            });
        }
        for (const QString& name : {QStringLiteral("debug.addWatchpoint"),
                                    QStringLiteral("debug.removeWatchpoint"),
                                    QStringLiteral("debug.listWatchpoints")}) {
            tools.append(QJsonObject{
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), name.endsWith(QStringLiteral("addWatchpoint"))
                    ? QStringLiteral("MCPデバッグ監視対象を追加")
                    : name.endsWith(QStringLiteral("removeWatchpoint"))
                        ? QStringLiteral("MCPデバッグ監視対象を削除")
                        : QStringLiteral("MCPデバッグ監視対象を一覧表示")},
                {QStringLiteral("parameters"), QJsonArray{
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("path")},
                                 {QStringLiteral("type"), QStringLiteral("string")}},
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("label")},
                                 {QStringLiteral("type"), QStringLiteral("string")}},
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("id")},
                                 {QStringLiteral("type"), QStringLiteral("int")}}
                }}
            });
        }
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.trace")},
            {QStringLiteral("description"), QStringLiteral("直近のフレーム・scope・event・crashトレースを取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.flow")},
            {QStringLiteral("description"), QStringLiteral("直近のトレースをMermaidシーケンス図として取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.rootCause")},
            {QStringLiteral("description"), QStringLiteral("最新crashまたはエラーイベントの根本原因候補を取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.diff")},
            {QStringLiteral("description"), QStringLiteral("2つのtrace frameのscope差分を取得")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("frameA")},
                             {QStringLiteral("type"), QStringLiteral("int")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("frameB")},
                             {QStringLiteral("type"), QStringLiteral("int")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.bisect")},
            {QStringLiteral("description"), QStringLiteral("観測済みtraceからscope出現frameの変更点候補を推定")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("goodFrame")},
                             {QStringLiteral("type"), QStringLiteral("int")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("badFrame")},
                             {QStringLiteral("type"), QStringLiteral("int")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("targetExpression")},
                             {QStringLiteral("type"), QStringLiteral("string")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.recipe")},
            {QStringLiteral("description"), QStringLiteral("Traceから最小再現手順候補を生成")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("maxSteps")},
                             {QStringLiteral("type"), QStringLiteral("int")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.listProperties")},
            {QStringLiteral("description"), QStringLiteral("登録済みプロパティのパスと値を取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.getProperty")},
            {QStringLiteral("description"), QStringLiteral("完全なプロパティパスで値を取得")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("path")},
                             {QStringLiteral("type"), QStringLiteral("string")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.query")},
            {QStringLiteral("description"), QStringLiteral("プロパティpath/typeを条件検索")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("where")},
                             {QStringLiteral("type"), QStringLiteral("string")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("limit")},
                             {QStringLiteral("type"), QStringLiteral("int")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("select")},
                             {QStringLiteral("type"), QStringLiteral("array<string>")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("orderBy")},
                             {QStringLiteral("type"), QStringLiteral("string")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.setProperty")},
            {QStringLiteral("description"), QStringLiteral("完全なプロパティパスで値を設定")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("path")},
                             {QStringLiteral("type"), QStringLiteral("string")}},
                QJsonObject{{QStringLiteral("name"), QStringLiteral("value")},
                             {QStringLiteral("type"), QStringLiteral("variant")}}
            }}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.evaluate")},
            {QStringLiteral("description"), QStringLiteral("ExpressionEvaluatorで式を評価")},
            {QStringLiteral("parameters"), QJsonArray{
                QJsonObject{{QStringLiteral("name"), QStringLiteral("expression")},
                             {QStringLiteral("type"), QStringLiteral("string")}}
            }}
        });
        for (const QString& name : {QStringLiteral("debug.patch.begin"),
                                    QStringLiteral("debug.patch.apply"),
                                    QStringLiteral("debug.patch.rollback"),
                                    QStringLiteral("debug.patch.commit")}) {
            tools.append(QJsonObject{
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), name.endsWith(QStringLiteral("begin"))
                    ? QStringLiteral("プロパティLive Patchセッションを開始")
                    : name.endsWith(QStringLiteral("apply"))
                        ? QStringLiteral("Live Patchを適用")
                        : name.endsWith(QStringLiteral("rollback"))
                            ? QStringLiteral("Live Patchをロールバック")
                            : QStringLiteral("Live Patchを確定")},
                {QStringLiteral("parameters"), QJsonArray{
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("path")},
                                 {QStringLiteral("type"), QStringLiteral("string")}},
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("value")},
                                 {QStringLiteral("type"), QStringLiteral("variant")}}
                }}
            });
        }
        for (const QString& name : {QStringLiteral("debug.performance"),
                                    QStringLiteral("debug.memory.list"),
                                    QStringLiteral("debug.memory.dump"),
                                    QStringLiteral("debug.memory.graph"),
                                    QStringLiteral("debug.memory.leaks"),
                                    QStringLiteral("debug.predict.risks"),
                                    QStringLiteral("debug.predict.autoWatch"),
                                    QStringLiteral("debug.stress.run"),
                                    QStringLiteral("debug.stress.result")}) {
            tools.append(QJsonObject{
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), name == QStringLiteral("debug.performance")
                    ? QStringLiteral("Trace frame timingからFPSとフレーム時間を取得")
                    : QStringLiteral("AI debug diagnostics")},
                {QStringLiteral("parameters"), QJsonArray{}}
            });
        }
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.renderGraph")},
            {QStringLiteral("description"), QStringLiteral("Traceから観測されたレンダーパス一覧を取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        tools.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("debug.getTimeline")},
            {QStringLiteral("description"), QStringLiteral("Traceに記録されたフレーム時系列を取得")},
            {QStringLiteral("parameters"), QJsonArray{}}
        });
        for (const QString& name : {QStringLiteral("debug.regression.capture"),
                                    QStringLiteral("debug.regression.compare"),
                                    QStringLiteral("debug.regression.detect")}) {
            tools.append(QJsonObject{
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), name.endsWith(QStringLiteral("capture"))
                    ? QStringLiteral("現在のtraceを回帰ベースラインとして保存")
                    : name.endsWith(QStringLiteral("detect"))
                        ? QStringLiteral("期待差分を除外して予期しない回帰を検出")
                        : QStringLiteral("回帰ベースラインと現在のtraceを比較")},
                {QStringLiteral("parameters"), [&name]() {
                    QJsonArray parameters{
                        QJsonObject{{QStringLiteral("name"), QStringLiteral("name")},
                                     {QStringLiteral("type"), QStringLiteral("string")}}
                    };
                    if (name.endsWith(QStringLiteral("detect"))) {
                        parameters.append(QJsonObject{
                            {QStringLiteral("name"), QStringLiteral("expectedAddedFrames")},
                            {QStringLiteral("type"), QStringLiteral("array<int>")}
                        });
                        parameters.append(QJsonObject{
                            {QStringLiteral("name"), QStringLiteral("expectedRemovedFrames")},
                            {QStringLiteral("type"), QStringLiteral("array<int>")}
                        });
                    }
                    return parameters;
                }()}
            });
        }
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
                entry[QStringLiteral("name")] = componentName + QStringLiteral(".") + methodName;
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
            const QString debugToolName = params.value(QStringLiteral("name")).toString().trimmed();
            if (debugToolName == QStringLiteral("debug.log")) {
                int count = params.value(QStringLiteral("arguments")).toObject()
                                .value(QStringLiteral("count")).toInt(200);
                count = std::clamp(count, 1, 1000);
                const auto logs = Logger::instance()->getLogs();
                QJsonArray entries;
                const int first = std::max(0, static_cast<int>(logs.size()) - count);
                for (int i = first; i < static_cast<int>(logs.size()); ++i) {
                    const auto& log = logs[static_cast<std::size_t>(i)];
                    entries.append(QJsonObject{
                        {QStringLiteral("timestamp"), log.timestamp.toString(Qt::ISODateWithMs)},
                        {QStringLiteral("level"), static_cast<int>(log.level)},
                        {QStringLiteral("message"), log.message},
                        {QStringLiteral("context"), log.context}
                    });
                }
                QJsonObject result;
                result[QStringLiteral("content")] = QStringLiteral("debug.log");
                result[QStringLiteral("structuredContent")] = QJsonObject{
                    {QStringLiteral("entries"), entries},
                    {QStringLiteral("count"), entries.size()}
                };
                return makeResponse(result);
            }
            if (debugToolName == QStringLiteral("debug.logCategory")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString category = arguments.value(QStringLiteral("category")).toString().trimmed();
                int count = std::clamp(arguments.value(QStringLiteral("count")).toInt(100), 1, 1000);
                const auto logs = Logger::instance()->getLogs();
                QJsonArray entries;
                for (auto it = logs.rbegin(); it != logs.rend() && entries.size() < count; ++it) {
                    if (!category.isEmpty() && !it->context.contains(category, Qt::CaseInsensitive)) {
                        continue;
                    }
                    entries.prepend(QJsonObject{
                        {QStringLiteral("timestamp"), it->timestamp.toString(Qt::ISODateWithMs)},
                        {QStringLiteral("level"), static_cast<int>(it->level)},
                        {QStringLiteral("message"), it->message},
                        {QStringLiteral("context"), it->context}
                    });
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.logCategory")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("category"), category},
                        {QStringLiteral("entries"), entries},
                        {QStringLiteral("count"), entries.size()}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.filterLog")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString query = arguments.value(QStringLiteral("query")).toString().trimmed();
                const int count = std::clamp(arguments.value(QStringLiteral("count")).toInt(100), 1, 1000);
                const auto logs = Logger::instance()->getLogs();
                QJsonArray entries;
                for (auto it = logs.rbegin(); it != logs.rend() && entries.size() < count; ++it) {
                    const QString text = it->message + QStringLiteral(" ") + it->context;
                    if (!query.isEmpty() && !text.contains(query, Qt::CaseInsensitive)) continue;
                    entries.prepend(QJsonObject{
                        {QStringLiteral("timestamp"), it->timestamp.toString(Qt::ISODateWithMs)},
                        {QStringLiteral("level"), static_cast<int>(it->level)},
                        {QStringLiteral("message"), it->message},
                        {QStringLiteral("context"), it->context}
                    });
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.filterLog")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("query"), query},
                        {QStringLiteral("entries"), entries},
                        {QStringLiteral("count"), entries.size()}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.getTools")) {
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.getTools")},
                    {QStringLiteral("structuredContent"), capabilityList()}
                });
            }
            if (debugToolName == QStringLiteral("debug.state") ||
                debugToolName == QStringLiteral("debug.pause") ||
                debugToolName == QStringLiteral("debug.resume") ||
                debugToolName == QStringLiteral("debug.getSelection") ||
                debugToolName == QStringLiteral("debug.getViewport") ||
                debugToolName == QStringLiteral("debug.compositionState") ||
                debugToolName == QStringLiteral("debug.layerState") ||
                debugToolName == QStringLiteral("debug.listCompositions") ||
                debugToolName == QStringLiteral("debug.listLayers") ||
                debugToolName == QStringLiteral("debug.renderQueue") ||
                debugToolName == QStringLiteral("debug.getFarm") ||
                debugToolName == QStringLiteral("debug.stepForward") ||
                debugToolName == QStringLiteral("debug.stepToFrame") ||
                debugToolName == QStringLiteral("debug.getColorPipeline") ||
                debugToolName == QStringLiteral("debug.getMaskPaths") ||
                debugToolName == QStringLiteral("debug.gpuMemory") ||
                debugToolName == QStringLiteral("debug.getRig")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString envPath = qEnvironmentVariable("ARTIFACT_DEBUG_MCP_STATE_FILE");
                const QString statePath = envPath.trimmed().isEmpty()
                    ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                          .filePath(QStringLiteral("ArtifactStudio/debug-mcp-state.json"))
                    : envPath;
                QFile stateFile(statePath);
                QJsonObject state;
                if (stateFile.open(QIODevice::ReadOnly)) {
                    QJsonParseError parseError;
                    const QJsonDocument document = QJsonDocument::fromJson(stateFile.readAll(), &parseError);
                    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                        state = document.object();
                    }
                }
                if (state.isEmpty()) {
                    state.insert(QStringLiteral("version"), 1);
                    state.insert(QStringLiteral("session"), QJsonObject{
                        {QStringLiteral("paused"), false},
                        {QStringLiteral("lastAction"), QStringLiteral("idle")}
                    });
                }
                if (debugToolName == QStringLiteral("debug.getSelection") ||
                    debugToolName == QStringLiteral("debug.getViewport") ||
                    debugToolName == QStringLiteral("debug.compositionState") ||
                    debugToolName == QStringLiteral("debug.layerState") ||
                    debugToolName == QStringLiteral("debug.listCompositions") ||
                    debugToolName == QStringLiteral("debug.listLayers") ||
                    debugToolName == QStringLiteral("debug.renderQueue") ||
                    debugToolName == QStringLiteral("debug.getFarm") ||
                    debugToolName == QStringLiteral("debug.getColorPipeline") ||
                    debugToolName == QStringLiteral("debug.getMaskPaths")) {
                    const QJsonObject snapshot = state.value(QStringLiteral("mockSnapshot")).toObject();
                    QJsonObject result;
                    if (debugToolName == QStringLiteral("debug.getSelection")) {
                        result = snapshot.value(QStringLiteral("selection")).toObject();
                    } else if (debugToolName == QStringLiteral("debug.getViewport")) {
                        result = snapshot.value(QStringLiteral("viewport")).toObject();
                    } else if (debugToolName == QStringLiteral("debug.compositionState")) {
                        for (const QString& key : {QStringLiteral("project"),
                                                    QStringLiteral("composition"),
                                                    QStringLiteral("playback"),
                                                    QStringLiteral("diagnostics")}) {
                            result.insert(key, snapshot.value(key));
                        }
                    } else if (debugToolName == QStringLiteral("debug.listCompositions")) {
                        const QJsonObject workspace = snapshot.value(QStringLiteral("workspace")).toObject();
                        QJsonArray compositions = workspace.value(QStringLiteral("compositions")).toArray();
                        if (compositions.isEmpty()) {
                            const QJsonObject current = snapshot.value(QStringLiteral("composition")).toObject();
                            if (!current.isEmpty()) compositions.append(current);
                        }
                        result.insert(QStringLiteral("compositions"), compositions);
                        result.insert(QStringLiteral("observed"), !compositions.isEmpty());
                    } else if (debugToolName == QStringLiteral("debug.listLayers")) {
                        const QJsonObject composition = snapshot.value(QStringLiteral("composition")).toObject();
                        const QJsonArray layers = composition.value(QStringLiteral("layers")).toArray();
                        result.insert(QStringLiteral("layers"), layers);
                        result.insert(QStringLiteral("observed"), !layers.isEmpty());
                    } else if (debugToolName == QStringLiteral("debug.renderQueue")) {
                        const QJsonObject queue = snapshot.value(QStringLiteral("renderQueue")).toObject();
                        result = queue;
                        result.insert(QStringLiteral("observed"), !queue.isEmpty());
                    } else if (debugToolName == QStringLiteral("debug.getFarm")) {
                        const QJsonObject workspace = snapshot.value(QStringLiteral("workspace")).toObject();
                        const QJsonObject farm = workspace.value(QStringLiteral("farm")).toObject();
                        result = farm;
                        result.insert(QStringLiteral("observed"), !farm.isEmpty());
                    } else if (debugToolName == QStringLiteral("debug.getColorPipeline")) {
                        const QJsonValue pipeline = snapshot.value(QStringLiteral("colorPipeline"));
                        result.insert(QStringLiteral("value"), pipeline);
                        result.insert(QStringLiteral("observed"), !pipeline.isUndefined() && !pipeline.isNull());
                    } else if (debugToolName == QStringLiteral("debug.getMaskPaths")) {
                        const QJsonValue masks = snapshot.value(QStringLiteral("maskPaths"));
                        result.insert(QStringLiteral("value"), masks);
                        result.insert(QStringLiteral("observed"), !masks.isUndefined() && !masks.isNull());
                    } else if (debugToolName == QStringLiteral("debug.gpuMemory")) {
                        const QJsonValue memory = snapshot.value(QStringLiteral("gpuMemory"));
                        result.insert(QStringLiteral("value"), memory);
                        result.insert(QStringLiteral("observed"), !memory.isUndefined() && !memory.isNull());
                    } else if (debugToolName == QStringLiteral("debug.getRig")) {
                        const QJsonValue rig = snapshot.value(QStringLiteral("rig"));
                        result.insert(QStringLiteral("value"), rig);
                        result.insert(QStringLiteral("observed"), !rig.isUndefined() && !rig.isNull());
                    } else {
                        for (const QString& key : {QStringLiteral("selection"),
                                                    QStringLiteral("properties"),
                                                    QStringLiteral("composition"),
                                                    QStringLiteral("playback")}) {
                            result.insert(key, snapshot.value(key));
                        }
                    }
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), debugToolName},
                        {QStringLiteral("structuredContent"), result}
                    });
                }
                if (debugToolName == QStringLiteral("debug.stepForward") ||
                    debugToolName == QStringLiteral("debug.stepToFrame")) {
                    QJsonObject updatedSession = state.value(QStringLiteral("session")).toObject();
                    updatedSession.insert(QStringLiteral("paused"), true);
                    if (debugToolName == QStringLiteral("debug.stepForward")) {
                        updatedSession.insert(QStringLiteral("lastAction"), QStringLiteral("mcp.stepForward"));
                        updatedSession.insert(QStringLiteral("stepFrames"),
                                              std::clamp(arguments.value(QStringLiteral("frames")).toInt(1), 1, 1000));
                    } else {
                        updatedSession.insert(QStringLiteral("lastAction"), QStringLiteral("mcp.stepToFrame"));
                        updatedSession.insert(QStringLiteral("targetFrame"),
                                              arguments.value(QStringLiteral("frame")).toInt());
                    }
                    state.insert(QStringLiteral("session"), updatedSession);
                    QDir().mkpath(QFileInfo(statePath).absolutePath());
                    if (stateFile.isOpen()) stateFile.close();
                    const bool written = stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
                    if (written) stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), debugToolName},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("written"), written},
                            {QStringLiteral("session"), updatedSession}
                        }}
                    });
                }
                if (debugToolName != QStringLiteral("debug.state")) {
                    QJsonObject session = state.value(QStringLiteral("session")).toObject();
                    const bool paused = debugToolName == QStringLiteral("debug.pause");
                    session.insert(QStringLiteral("paused"), paused);
                    session.insert(QStringLiteral("lastAction"), paused
                        ? QStringLiteral("mcp.pause") : QStringLiteral("mcp.resume"));
                    state.insert(QStringLiteral("session"), session);
                    QDir().mkpath(QFileInfo(statePath).absolutePath());
                    if (stateFile.isOpen()) {
                        stateFile.close();
                    }
                    if (stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                    }
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), debugToolName},
                    {QStringLiteral("structuredContent"), state}
                });
            }
            if (debugToolName == QStringLiteral("debug.trace")) {
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.trace")},
                    {QStringLiteral("structuredContent"),
                     ArtifactCore::TraceRecorder::instance().snapshot().toJson()}
                });
            }
            if (debugToolName == QStringLiteral("debug.addDataBreakpoint") ||
                debugToolName == QStringLiteral("debug.removeDataBreakpoint") ||
                debugToolName == QStringLiteral("debug.listDataBreakpoints")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString envPath = qEnvironmentVariable("ARTIFACT_DEBUG_MCP_STATE_FILE");
                const QString statePath = envPath.trimmed().isEmpty()
                    ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                          .filePath(QStringLiteral("ArtifactStudio/debug-mcp-state.json"))
                    : envPath;
                QFile stateFile(statePath);
                QJsonObject state;
                if (stateFile.open(QIODevice::ReadOnly)) {
                    const QJsonDocument document = QJsonDocument::fromJson(stateFile.readAll());
                    if (document.isObject()) state = document.object();
                }
                QJsonArray conditions = state.value(QStringLiteral("breakConditions")).toArray();
                int nextId = state.value(QStringLiteral("nextConditionId")).toInt(1);
                if (debugToolName.endsWith(QStringLiteral("listDataBreakpoints"))) {
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.listDataBreakpoints")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("breakConditions"), conditions},
                            {QStringLiteral("count"), conditions.size()}
                        }}
                    });
                }
                if (debugToolName.endsWith(QStringLiteral("removeDataBreakpoint"))) {
                    const int id = arguments.value(QStringLiteral("id")).toInt(-1);
                    QJsonArray kept;
                    bool removed = false;
                    for (const auto& condition : conditions) {
                        if (condition.toObject().value(QStringLiteral("id")).toInt(-1) == id) {
                            removed = true;
                        } else {
                            kept.append(condition);
                        }
                    }
                    conditions = kept;
                    state.insert(QStringLiteral("breakConditions"), conditions);
                    state.insert(QStringLiteral("nextConditionId"), nextId);
                    QDir().mkpath(QFileInfo(statePath).absolutePath());
                    if (stateFile.isOpen()) stateFile.close();
                    if (stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                    }
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.removeDataBreakpoint")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("id"), id}, {QStringLiteral("removed"), removed}
                        }}
                    });
                }
                QJsonObject condition{
                    {QStringLiteral("id"), nextId++},
                    {QStringLiteral("kind"), arguments.value(QStringLiteral("kind")).toString()},
                    {QStringLiteral("value"), arguments.value(QStringLiteral("value"))},
                    {QStringLiteral("enabled"), true}
                };
                conditions.append(condition);
                state.insert(QStringLiteral("breakConditions"), conditions);
                state.insert(QStringLiteral("nextConditionId"), nextId);
                QDir().mkpath(QFileInfo(statePath).absolutePath());
                if (stateFile.isOpen()) stateFile.close();
                bool written = stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
                if (written) stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.addDataBreakpoint")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("condition"), condition}, {QStringLiteral("written"), written}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.addWatchpoint") ||
                debugToolName == QStringLiteral("debug.removeWatchpoint") ||
                debugToolName == QStringLiteral("debug.listWatchpoints")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString envPath = qEnvironmentVariable("ARTIFACT_DEBUG_MCP_STATE_FILE");
                const QString statePath = envPath.trimmed().isEmpty()
                    ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                          .filePath(QStringLiteral("ArtifactStudio/debug-mcp-state.json"))
                    : envPath;
                QFile stateFile(statePath);
                QJsonObject state;
                if (stateFile.open(QIODevice::ReadOnly)) {
                    const QJsonDocument document = QJsonDocument::fromJson(stateFile.readAll());
                    if (document.isObject()) state = document.object();
                }
                QJsonArray descriptors = state.value(QStringLiteral("watchDescriptors")).toArray();
                int nextId = state.value(QStringLiteral("nextWatchId")).toInt(1);
                if (debugToolName.endsWith(QStringLiteral("listWatchpoints"))) {
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.listWatchpoints")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("watchDescriptors"), descriptors},
                            {QStringLiteral("count"), descriptors.size()}
                        }}
                    });
                }
                if (debugToolName.endsWith(QStringLiteral("removeWatchpoint"))) {
                    const int id = arguments.value(QStringLiteral("id")).toInt(-1);
                    QJsonArray kept;
                    bool removed = false;
                    for (const auto& descriptor : descriptors) {
                        if (descriptor.toObject().value(QStringLiteral("id")).toInt(-1) == id) {
                            removed = true;
                        } else {
                            kept.append(descriptor);
                        }
                    }
                    descriptors = kept;
                    state.insert(QStringLiteral("watchDescriptors"), descriptors);
                    state.insert(QStringLiteral("nextWatchId"), nextId);
                    QDir().mkpath(QFileInfo(statePath).absolutePath());
                    if (stateFile.isOpen()) stateFile.close();
                    if (stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                    }
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.removeWatchpoint")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("id"), id}, {QStringLiteral("removed"), removed}
                        }}
                    });
                }
                QJsonObject descriptor{
                    {QStringLiteral("id"), nextId++},
                    {QStringLiteral("path"), arguments.value(QStringLiteral("path")).toString()},
                    {QStringLiteral("label"), arguments.value(QStringLiteral("label")).toString()},
                    {QStringLiteral("enabled"), true}
                };
                descriptors.append(descriptor);
                state.insert(QStringLiteral("watchDescriptors"), descriptors);
                state.insert(QStringLiteral("nextWatchId"), nextId);
                QDir().mkpath(QFileInfo(statePath).absolutePath());
                if (stateFile.isOpen()) stateFile.close();
                const bool written = stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
                if (written) stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.addWatchpoint")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("watchpoint"), descriptor}, {QStringLiteral("written"), written}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.flow")) {
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QString mermaid = QStringLiteral("sequenceDiagram\n");
                mermaid += QStringLiteral("  participant App as ArtifactStudio\n");
                int emitted = 0;
                for (auto frameIt = trace.frames.rbegin();
                     frameIt != trace.frames.rend() && emitted < 40; ++frameIt) {
                    const QString frameName = QStringLiteral("Frame_%1")
                        .arg(QString::number(frameIt->frameIndex));
                    mermaid += QStringLiteral("  App->>App: %1\n").arg(frameName);
                    for (const auto& lane : frameIt->lanes) {
                        for (const auto& scope : lane.scopes) {
                            const QString participant = QStringLiteral("%1_%2")
                                .arg(QStringLiteral("P"), QString::number(emitted));
                            QString laneName = lane.laneName;
                            QString scopeName = scope.name;
                            laneName.replace(QChar('\n'), QChar(' '));
                            scopeName.replace(QChar('\n'), QChar(' '));
                            mermaid += QStringLiteral("  participant %1 as %2\n")
                                .arg(participant, laneName);
                            mermaid += QStringLiteral("  App->>%1: %2\n")
                                .arg(participant, scopeName);
                            ++emitted;
                            if (emitted >= 40) {
                                break;
                            }
                        }
                        if (emitted >= 40) {
                            break;
                        }
                    }
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), mermaid},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("format"), QStringLiteral("mermaid")},
                        {QStringLiteral("diagram"), mermaid},
                        {QStringLiteral("scopeCount"), emitted}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.rootCause")) {
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QJsonObject root;
                QJsonArray relatedEvents;
                QString summary = QStringLiteral("No crash or trace event recorded.");
                QString stack;
                QString threadName;
                qint64 anchorMs = 0;
                if (!trace.crashes.empty()) {
                    const auto& crash = trace.crashes.back();
                    summary = crash.summary;
                    stack = crash.stack;
                    threadName = crash.threadName;
                    anchorMs = crash.timestampMs;
                    root.insert(QStringLiteral("source"), QStringLiteral("crash"));
                } else if (!trace.events.empty()) {
                    const auto& event = trace.events.back();
                    summary = event.name;
                    anchorMs = event.startNs / 1000000LL;
                    root.insert(QStringLiteral("source"), QStringLiteral("trace-event"));
                } else {
                    root.insert(QStringLiteral("source"), QStringLiteral("none"));
                }
                for (auto it = trace.events.rbegin();
                     it != trace.events.rend() && relatedEvents.size() < 20; ++it) {
                    const qint64 eventMs = it->startNs / 1000000LL;
                    if (anchorMs == 0 || eventMs <= anchorMs) {
                        relatedEvents.prepend(QJsonObject{
                            {QStringLiteral("kind"), toString(it->kind)},
                            {QStringLiteral("domain"), toString(it->domain)},
                            {QStringLiteral("name"), it->name},
                            {QStringLiteral("detail"), it->detail},
                            {QStringLiteral("frameIndex"), it->frameIndex},
                            {QStringLiteral("timestampMs"), eventMs}
                        });
                    }
                }
                root.insert(QStringLiteral("summary"), summary);
                root.insert(QStringLiteral("stack"), stack);
                root.insert(QStringLiteral("threadName"), threadName);
                root.insert(QStringLiteral("relatedEvents"), relatedEvents);
                root.insert(QStringLiteral("confidence"), relatedEvents.isEmpty()
                    ? QStringLiteral("low") : QStringLiteral("heuristic"));
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.rootCause")},
                    {QStringLiteral("structuredContent"), root}
                });
            }
            if (debugToolName == QStringLiteral("debug.diff")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const int frameA = arguments.value(QStringLiteral("frameA")).toInt(-1);
                const int frameB = arguments.value(QStringLiteral("frameB")).toInt(-1);
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                const TraceFrameTimelineRecord* a = nullptr;
                const TraceFrameTimelineRecord* b = nullptr;
                for (const auto& frame : trace.frames) {
                    if (frame.frameIndex == frameA) a = &frame;
                    if (frame.frameIndex == frameB) b = &frame;
                }
                auto scopeNames = [](const TraceFrameTimelineRecord* frame) {
                    QSet<QString> names;
                    if (!frame) return names;
                    for (const auto& lane : frame->lanes) {
                        for (const auto& scope : lane.scopes) {
                            names.insert(lane.laneName + QStringLiteral("/") + scope.name);
                        }
                    }
                    return names;
                };
                const QSet<QString> namesA = scopeNames(a);
                const QSet<QString> namesB = scopeNames(b);
                QJsonArray added;
                QJsonArray removed;
                for (const auto& name : namesB) {
                    if (!namesA.contains(name)) added.append(name);
                }
                for (const auto& name : namesA) {
                    if (!namesB.contains(name)) removed.append(name);
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.diff")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("frameA"), frameA},
                        {QStringLiteral("frameB"), frameB},
                        {QStringLiteral("frameAFound"), a != nullptr},
                        {QStringLiteral("frameBFound"), b != nullptr},
                        {QStringLiteral("addedScopes"), added},
                        {QStringLiteral("removedScopes"), removed}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.bisect")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const int goodFrame = arguments.value(QStringLiteral("goodFrame")).toInt(-1);
                const int badFrame = arguments.value(QStringLiteral("badFrame")).toInt(-1);
                const QString target = arguments.value(QStringLiteral("targetExpression")).toString().trimmed();
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                int firstMatchingFrame = -1;
                for (const auto& frame : trace.frames) {
                    if (frame.frameIndex < std::min(goodFrame, badFrame) ||
                        frame.frameIndex > std::max(goodFrame, badFrame)) continue;
                    bool matched = target.isEmpty();
                    for (const auto& lane : frame.lanes) {
                        for (const auto& scope : lane.scopes) {
                            if (lane.laneName.contains(target, Qt::CaseInsensitive) ||
                                scope.name.contains(target, Qt::CaseInsensitive)) {
                                matched = true;
                            }
                        }
                    }
                    if (matched && (firstMatchingFrame < 0 || frame.frameIndex < firstMatchingFrame)) {
                        firstMatchingFrame = frame.frameIndex;
                    }
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.bisect")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("goodFrame"), goodFrame},
                        {QStringLiteral("badFrame"), badFrame},
                        {QStringLiteral("targetExpression"), target},
                        {QStringLiteral("candidateFrame"), firstMatchingFrame},
                        {QStringLiteral("heuristic"), true}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.recipe")) {
                const int maxSteps = std::clamp(
                    params.value(QStringLiteral("arguments")).toObject()
                        .value(QStringLiteral("maxSteps")).toInt(20), 1, 100);
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QJsonArray steps;
                QSet<QString> emitted;
                for (const auto& event : trace.events) {
                    const QString label = event.name.trimmed().isEmpty()
                        ? event.detail : event.name;
                    if (label.isEmpty() || emitted.contains(label)) continue;
                    emitted.insert(label);
                    steps.append(QJsonObject{
                        {QStringLiteral("action"), label},
                        {QStringLiteral("kind"), toString(event.kind)},
                        {QStringLiteral("frameIndex"), event.frameIndex}
                    });
                    if (steps.size() >= maxSteps) break;
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.recipe")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("title"), QStringLiteral("Trace-derived reproduction candidate")},
                        {QStringLiteral("heuristic"), true},
                        {QStringLiteral("steps"), steps},
                        {QStringLiteral("count"), steps.size()}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.listProperties") ||
                debugToolName == QStringLiteral("debug.getProperty") ||
                debugToolName == QStringLiteral("debug.setProperty")) {
                const auto arguments = params.value(QStringLiteral("arguments")).toObject();
                const auto properties = PropertyRegistryReadOnlyAdapter::queryAllProperties();
                const QString requestedPath = arguments
                    .value(QStringLiteral("path")).toString().trimmed();
                const QVariant requestedValue = arguments.value(QStringLiteral("value")).toVariant();
                QJsonArray values;
                QJsonObject found;
                bool changed = false;
                for (const auto& property : ArtifactCore::globalPropertyRegistry().enumerate()) {
                    const QString path = property.ownerPath + QStringLiteral(".") + property.propertyName;
                    if ((debugToolName == QStringLiteral("debug.getProperty") ||
                         debugToolName == QStringLiteral("debug.setProperty")) &&
                        path != requestedPath) {
                        continue;
                    }
            if (debugToolName == QStringLiteral("debug.setProperty")) {
                        PropertyOwnerDescriptor descriptor;
                        const bool ownerWritable =
                            globalPropertyRegistry().tryGetOwner(property.ownerPath, &descriptor) &&
                            !descriptor.readOnly;
                        if (property.property && ownerWritable && requestedValue.isValid()) {
                            property.property->setValue(requestedValue);
                            changed = true;
                        }
                    }
                    const QVariant currentValue = property.property
                        ? property.property->getValue() : QVariant{};
                    QJsonObject value{
                        {QStringLiteral("path"), path},
                        {QStringLiteral("ownerPath"), property.ownerPath},
                        {QStringLiteral("propertyName"), property.propertyName},
                        {QStringLiteral("type"), property.propertyType},
                        {QStringLiteral("value"), QJsonValue::fromVariant(currentValue)},
                        {QStringLiteral("readOnly"), false}
                    };
                    if (debugToolName == QStringLiteral("debug.getProperty") ||
                        debugToolName == QStringLiteral("debug.setProperty")) {
                        found = value;
                        break;
                    }
                    values.append(value);
                }
                QJsonObject structured;
                if (debugToolName == QStringLiteral("debug.getProperty") ||
                    debugToolName == QStringLiteral("debug.setProperty")) {
                    structured.insert(QStringLiteral("found"), !found.isEmpty() || changed);
                    structured.insert(QStringLiteral("changed"), changed);
                    structured.insert(QStringLiteral("property"), found);
                } else {
                    structured.insert(QStringLiteral("properties"), values);
                    structured.insert(QStringLiteral("count"), values.size());
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), debugToolName},
                    {QStringLiteral("structuredContent"), structured}
                });
            }
            if (debugToolName == QStringLiteral("debug.patch.begin") ||
                debugToolName == QStringLiteral("debug.patch.apply") ||
                debugToolName == QStringLiteral("debug.patch.rollback") ||
                debugToolName == QStringLiteral("debug.patch.commit")) {
                static QHash<QString, QVariant> patchOriginalValues;
                static bool patchActive = false;
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString path = arguments.value(QStringLiteral("path")).toString().trimmed();
                auto findProperty = [&path]() -> AbstractPropertyPtr {
                    for (const auto& handle : globalPropertyRegistry().enumerate()) {
                        if (handle.path() == path) return handle.property;
                    }
                    return {};
                };
                if (debugToolName.endsWith(QStringLiteral("begin"))) {
                    patchOriginalValues.clear();
                    patchActive = true;
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.patch.begin")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("active"), true}
                        }}
                    });
                }
                if (!patchActive) {
                    return makeError(-32602, QStringLiteral("No active Live Patch session"));
                }
                if (debugToolName.endsWith(QStringLiteral("apply"))) {
                    const auto property = findProperty();
                    if (!property || arguments.value(QStringLiteral("value")).isUndefined()) {
                        return makeError(-32602, QStringLiteral("Invalid patch path or value"));
                    }
                    if (!patchOriginalValues.contains(path)) {
                        patchOriginalValues.insert(path, property->getValue());
                    }
                    property->setValue(arguments.value(QStringLiteral("value")).toVariant());
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.patch.apply")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("path"), path},
                            {QStringLiteral("value"), QJsonValue::fromVariant(property->getValue())}
                        }}
                    });
                }
                if (debugToolName.endsWith(QStringLiteral("rollback"))) {
                    for (auto it = patchOriginalValues.cbegin(); it != patchOriginalValues.cend(); ++it) {
                        const auto property = [&it]() -> AbstractPropertyPtr {
                            for (const auto& handle : globalPropertyRegistry().enumerate()) {
                                if (handle.path() == it.key()) return handle.property;
                            }
                            return {};
                        }();
                        if (property) property->setValue(it.value());
                    }
                    patchOriginalValues.clear();
                    patchActive = false;
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.patch.rollback")},
                        {QStringLiteral("structuredContent"), QJsonObject{{QStringLiteral("active"), false}}}
                    });
                }
                patchOriginalValues.clear();
                patchActive = false;
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.patch.commit")},
                    {QStringLiteral("structuredContent"), QJsonObject{{QStringLiteral("active"), false}}}
                });
            }
            if (debugToolName == QStringLiteral("debug.evaluate")) {
                const QString expression = params.value(QStringLiteral("arguments")).toObject()
                    .value(QStringLiteral("expression")).toString();
                ExpressionEvaluator evaluator;
                evaluator.registerStandardFunctions();
                const ExpressionValue value = evaluator.evaluate(expression.toStdString());
                const auto text = value.toString();
                const QString error = QString::fromUtf8(evaluator.getError().c_str());
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QString::fromUtf8(text.data(),
                                                                   static_cast<int>(text.length()))},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("expression"), expression},
                        {QStringLiteral("value"), QString::fromUtf8(text.data(),
                                                                      static_cast<int>(text.length()))},
                        {QStringLiteral("type"), static_cast<int>(value.type())},
                        {QStringLiteral("hasError"), evaluator.hasError()},
                        {QStringLiteral("error"), error}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.query")) {
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString where = arguments.value(QStringLiteral("where")).toString().trimmed();
                const int limit = std::clamp(arguments.value(QStringLiteral("limit")).toInt(100), 1, 1000);
                const QString orderBy = arguments.value(QStringLiteral("orderBy")).toString().trimmed();
                const QJsonArray select = arguments.value(QStringLiteral("select")).toArray();
                QVector<QJsonObject> matchObjects;
                for (const auto& property : globalPropertyRegistry().enumerate()) {
                    const QString path = property.path();
                    const QString type = property.property ? QString::number(
                        static_cast<int>(property.property->getType())) : QStringLiteral("unknown");
                    if (!where.isEmpty() &&
                        !path.contains(where, Qt::CaseInsensitive) &&
                        !type.contains(where, Qt::CaseInsensitive)) {
                        continue;
                    }
                    matchObjects.append(QJsonObject{
                        {QStringLiteral("path"), path},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("value"), property.property
                            ? QJsonValue::fromVariant(property.property->getValue()) : QJsonValue{}}
                    });
                }
                if (orderBy == QStringLiteral("path") || orderBy == QStringLiteral("type")) {
                    std::sort(matchObjects.begin(), matchObjects.end(), [&orderBy](
                        const QJsonObject& lhs, const QJsonObject& rhs) {
                        return lhs.value(orderBy).toString() < rhs.value(orderBy).toString();
                    });
                }
                QJsonArray matches;
                for (const auto& object : matchObjects) {
                    QJsonObject selected = object;
                    if (!select.isEmpty()) {
                        selected = {};
                        for (const auto& field : select) {
                            const QString fieldName = field.toString();
                            if (object.contains(fieldName)) selected.insert(fieldName, object.value(fieldName));
                        }
                    }
                    matches.append(selected);
                    if (matches.size() >= limit) break;
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.query")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("where"), where},
                        {QStringLiteral("orderBy"), orderBy},
                        {QStringLiteral("matches"), matches},
                        {QStringLiteral("count"), matches.size()}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.performance")) {
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QJsonArray samples;
                double totalMs = 0.0;
                double lastMs = 0.0;
                int sampleCount = 0;
                for (auto it = trace.frames.rbegin();
                     it != trace.frames.rend() && sampleCount < 120; ++it) {
                    const double durationMs = static_cast<double>(it->frameEndNs - it->frameStartNs) / 1000000.0;
                    if (durationMs < 0.0) continue;
                    if (sampleCount == 0) lastMs = durationMs;
                    totalMs += durationMs;
                    samples.prepend(QJsonObject{
                        {QStringLiteral("frameIndex"), it->frameIndex},
                        {QStringLiteral("frameMs"), durationMs}
                    });
                    ++sampleCount;
                }
                const double averageMs = sampleCount > 0 ? totalMs / sampleCount : 0.0;
                const double fps = averageMs > 0.0 ? 1000.0 / averageMs : 0.0;
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.performance")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("lastFrameMs"), lastMs},
                        {QStringLiteral("averageFrameMs"), averageMs},
                        {QStringLiteral("fps"), fps},
                        {QStringLiteral("sampleCount"), sampleCount},
                        {QStringLiteral("samples"), samples}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.predict.risks") ||
                debugToolName == QStringLiteral("debug.predict.autoWatch")) {
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QHash<QString, int> scopeCounts;
                for (const auto& frame : trace.frames) {
                    for (const auto& lane : frame.lanes) {
                        for (const auto& scope : lane.scopes) {
                            if (!scope.name.trimmed().isEmpty()) ++scopeCounts[scope.name];
                        }
                    }
                }
                QJsonArray risks;
                for (auto it = scopeCounts.cbegin(); it != scopeCounts.cend(); ++it) {
                    const float score = std::min(1.0f, 0.25f + static_cast<float>(it.value()) / 100.0f);
                    risks.append(QJsonObject{
                        {QStringLiteral("fileOrComponent"), it.key()},
                        {QStringLiteral("score"), score},
                        {QStringLiteral("reason"), QStringLiteral("Repeated trace activity")},
                        {QStringLiteral("suggestedWatchTargets"), QJsonArray{it.key()}}});
                }
                int autoWatchAdded = 0;
                if (debugToolName == QStringLiteral("debug.predict.autoWatch")) {
                    const QString envPath = qEnvironmentVariable("ARTIFACT_DEBUG_MCP_STATE_FILE");
                    const QString statePath = envPath.trimmed().isEmpty()
                        ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                              .filePath(QStringLiteral("ArtifactStudio/debug-mcp-state.json"))
                        : envPath;
                    QFile stateFile(statePath);
                    QJsonObject state;
                    if (stateFile.open(QIODevice::ReadOnly)) {
                        const QJsonDocument document = QJsonDocument::fromJson(stateFile.readAll());
                        if (document.isObject()) state = document.object();
                    }
                    QJsonArray descriptors = state.value(QStringLiteral("watchDescriptors")).toArray();
                    int nextId = state.value(QStringLiteral("nextWatchId")).toInt(1);
                    for (const auto& risk : risks) {
                        const QString path = risk.toObject().value(QStringLiteral("fileOrComponent")).toString();
                        bool exists = false;
                        for (const auto& descriptor : descriptors) {
                            if (descriptor.toObject().value(QStringLiteral("path")).toString() == path) {
                                exists = true;
                                break;
                            }
                        }
                        if (exists) continue;
                        descriptors.append(QJsonObject{
                            {QStringLiteral("id"), nextId++},
                            {QStringLiteral("path"), path},
                            {QStringLiteral("label"), QStringLiteral("predictive")},
                            {QStringLiteral("enabled"), true}});
                        ++autoWatchAdded;
                    }
                    state.insert(QStringLiteral("watchDescriptors"), descriptors);
                    state.insert(QStringLiteral("nextWatchId"), nextId);
                    QDir().mkpath(QFileInfo(statePath).absolutePath());
                    if (stateFile.isOpen()) stateFile.close();
                    if (stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        stateFile.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
                    }
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), debugToolName},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("risks"), risks},
                        {QStringLiteral("count"), risks.size()},
                        {QStringLiteral("observed"), true},
                        {QStringLiteral("heuristic"), true},
                        {QStringLiteral("autoWatchApplied"),
                            debugToolName == QStringLiteral("debug.predict.autoWatch")},
                        {QStringLiteral("autoWatchAdded"), autoWatchAdded}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.stress.run") ||
                debugToolName == QStringLiteral("debug.stress.result")) {
                static QJsonObject stressResult{
                    {QStringLiteral("passed"), false},
                    {QStringLiteral("totalIterations"), 0},
                    {QStringLiteral("failedAtIteration"), -1},
                    {QStringLiteral("failureReason"), QStringLiteral("No stress run has been started")},
                    {QStringLiteral("crashCount"), 0},
                    {QStringLiteral("peakMemoryBytes"), 0}};
                if (debugToolName == QStringLiteral("debug.stress.run")) {
                    const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                    const int repeatCount = std::clamp(arguments.value(QStringLiteral("repeatCount")).toInt(1), 1, 100000);
                    const QJsonArray steps = arguments.value(QStringLiteral("steps")).toArray();
                    int completedIterations = 0;
                    int failedAt = -1;
                    QString failureReason;
                    for (int iteration = 0; iteration < repeatCount && failedAt < 0; ++iteration) {
                        for (const auto& step : steps) {
                            const QString expression = step.isObject()
                                ? step.toObject().value(QStringLiteral("expression")).toString()
                                : step.toString();
                            if (expression.trimmed().isEmpty()) continue;
                            ExpressionEvaluator evaluator;
                            evaluator.registerStandardFunctions();
                            evaluator.evaluate(expression.toStdString());
                            if (evaluator.hasError()) {
                                failedAt = iteration;
                                failureReason = QString::fromUtf8(evaluator.getError().c_str());
                                break;
                            }
                        }
                        if (failedAt < 0) ++completedIterations;
                    }
                    stressResult.insert(QStringLiteral("passed"), failedAt < 0);
                    stressResult.insert(QStringLiteral("totalIterations"), completedIterations);
                    stressResult.insert(QStringLiteral("failedAtIteration"), failedAt);
                    stressResult.insert(QStringLiteral("failureReason"), failureReason);
                    stressResult.insert(QStringLiteral("accepted"), true);
                    stressResult.insert(QStringLiteral("steps"), steps);
                    stressResult.insert(QStringLiteral("observed"), true);
                    stressResult.insert(QStringLiteral("execution"), QStringLiteral("expression-evaluator"));
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), debugToolName},
                    {QStringLiteral("structuredContent"), stressResult}
                });
            }
            if (debugToolName == QStringLiteral("debug.memory.list") ||
                debugToolName == QStringLiteral("debug.memory.dump") ||
                debugToolName == QStringLiteral("debug.memory.graph") ||
                debugToolName == QStringLiteral("debug.memory.leaks")) {
                const auto identities = Artifact::Diagnostics::DebugIdentity::snapshotAll();
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const uint64_t requestedId = arguments.value(QStringLiteral("id")).toVariant().toULongLong();
                QJsonArray objects;
                for (const auto& identity : identities) {
                    if (requestedId != 0 && identity.id != requestedId) continue;
                    objects.append(QJsonObject{
                        {QStringLiteral("id"), QString::number(identity.id)},
                        {QStringLiteral("ownerId"), QString::number(identity.ownerId)},
                        {QStringLiteral("type"), identity.typeName},
                        {QStringLiteral("name"), identity.name},
                        {QStringLiteral("ownerName"), identity.ownerName},
                        {QStringLiteral("creationFile"), identity.creationFile},
                        {QStringLiteral("creationFunction"), identity.creationFunction},
                        {QStringLiteral("creationLine"), static_cast<int>(identity.creationLine)}});
                }
                QJsonObject structured{
                    {QStringLiteral("objects"), objects},
                    {QStringLiteral("count"), objects.size()},
                    {QStringLiteral("observed"), true}};
                if (debugToolName == QStringLiteral("debug.memory.graph")) {
                    QJsonArray edges;
                    for (const auto& identity : identities) {
                        if (identity.ownerId == 0) continue;
                        edges.append(QJsonObject{
                            {QStringLiteral("ownerId"), QString::number(identity.ownerId)},
                            {QStringLiteral("objectId"), QString::number(identity.id)}});
                    }
                    structured.insert(QStringLiteral("edges"), edges);
                }
                if (debugToolName == QStringLiteral("debug.memory.leaks")) {
                    structured.insert(QStringLiteral("leaks"), objects);
                    structured.insert(QStringLiteral("heuristic"), true);
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), debugToolName},
                    {QStringLiteral("structuredContent"), structured}
                });
            }
            if (debugToolName == QStringLiteral("debug.renderGraph")) {
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QSet<QString> passNames;
                for (const auto& frame : trace.frames) {
                    for (const auto& lane : frame.lanes) {
                        if (!lane.laneName.trimmed().isEmpty()) {
                            passNames.insert(lane.laneName);
                        }
                        for (const auto& scope : lane.scopes) {
                            if (!scope.name.trimmed().isEmpty()) {
                                passNames.insert(scope.name);
                            }
                        }
                    }
                }
                QStringList sortedPassNames = passNames.values();
                std::sort(sortedPassNames.begin(), sortedPassNames.end());
                QJsonArray passes;
                for (const auto& name : sortedPassNames) {
                    passes.append(QJsonObject{{QStringLiteral("name"), name}});
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.renderGraph")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("observed"), !passes.isEmpty()},
                        {QStringLiteral("passes"), passes},
                        {QStringLiteral("count"), passes.size()}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.getTimeline")) {
                const auto trace = ArtifactCore::TraceRecorder::instance().snapshot();
                QJsonArray frames;
                for (const auto& frame : trace.frames) {
                    frames.append(QJsonObject{
                        {QStringLiteral("frameIndex"), frame.frameIndex},
                        {QStringLiteral("startNs"), frame.frameStartNs},
                        {QStringLiteral("endNs"), frame.frameEndNs},
                        {QStringLiteral("durationMs"),
                         static_cast<double>(frame.frameEndNs - frame.frameStartNs) / 1000000.0},
                        {QStringLiteral("laneCount"), static_cast<int>(frame.lanes.size())}
                    });
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.getTimeline")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("frames"), frames},
                        {QStringLiteral("count"), frames.size()}
                    }}
                });
            }
            if (debugToolName == QStringLiteral("debug.regression.capture") ||
                debugToolName == QStringLiteral("debug.regression.compare") ||
                debugToolName == QStringLiteral("debug.regression.detect")) {
                static QHash<QString, TraceSnapshot> baselines;
                const QJsonObject arguments = params.value(QStringLiteral("arguments")).toObject();
                const QString baselineName = arguments.value(QStringLiteral("name")).toString().trimmed();
                if (baselineName.isEmpty()) {
                    return makeError(-32602, QStringLiteral("debug.regression requires a non-empty name"));
                }
                if (debugToolName.endsWith(QStringLiteral("capture"))) {
                    baselines.insert(baselineName, ArtifactCore::TraceRecorder::instance().snapshot());
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.regression.capture")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("name"), baselineName},
                            {QStringLiteral("captured"), true}
                        }}
                    });
                }
                if (!baselines.contains(baselineName)) {
                    return makeError(-32602, QStringLiteral("Unknown regression baseline: ") + baselineName);
                }
                const auto& baseline = baselines.value(baselineName);
                const auto current = ArtifactCore::TraceRecorder::instance().snapshot();
                auto frameSet = [](const TraceSnapshot& snapshot) {
                    QSet<int> frames;
                    for (const auto& frame : snapshot.frames) frames.insert(frame.frameIndex);
                    return frames;
                };
                const QSet<int> baselineFrames = frameSet(baseline);
                const QSet<int> currentFrames = frameSet(current);
                QJsonArray addedFrames;
                QJsonArray removedFrames;
                for (const int frame : currentFrames) {
                    if (!baselineFrames.contains(frame)) addedFrames.append(frame);
                }
                for (const int frame : baselineFrames) {
                    if (!currentFrames.contains(frame)) removedFrames.append(frame);
                }
                if (debugToolName.endsWith(QStringLiteral("detect"))) {
                    const QSet<int> expectedAdded = QSet<int>::fromList(
                        [&arguments]() {
                            QList<int> result;
                            for (const auto& value : arguments.value(QStringLiteral("expectedAddedFrames")).toArray()) {
                                result.append(value.toInt());
                            }
                            return result;
                        }());
                    const QSet<int> expectedRemoved = QSet<int>::fromList(
                        [&arguments]() {
                            QList<int> result;
                            for (const auto& value : arguments.value(QStringLiteral("expectedRemovedFrames")).toArray()) {
                                result.append(value.toInt());
                            }
                            return result;
                        }());
                    QJsonArray unexpectedAdded;
                    QJsonArray unexpectedRemoved;
                    for (const auto& value : addedFrames) {
                        if (!expectedAdded.contains(value.toInt())) unexpectedAdded.append(value);
                    }
                    for (const auto& value : removedFrames) {
                        if (!expectedRemoved.contains(value.toInt())) unexpectedRemoved.append(value);
                    }
                    return makeResponse(QJsonObject{
                        {QStringLiteral("content"), QStringLiteral("debug.regression.detect")},
                        {QStringLiteral("structuredContent"), QJsonObject{
                            {QStringLiteral("name"), baselineName},
                            {QStringLiteral("hasRegression"), !unexpectedAdded.isEmpty() || !unexpectedRemoved.isEmpty()},
                            {QStringLiteral("unexpectedAddedFrames"), unexpectedAdded},
                            {QStringLiteral("unexpectedRemovedFrames"), unexpectedRemoved}
                        }}
                    });
                }
                return makeResponse(QJsonObject{
                    {QStringLiteral("content"), QStringLiteral("debug.regression.compare")},
                    {QStringLiteral("structuredContent"), QJsonObject{
                        {QStringLiteral("name"), baselineName},
                        {QStringLiteral("hasRegression"), !addedFrames.isEmpty() || !removedFrames.isEmpty()},
                        {QStringLiteral("addedFrames"), addedFrames},
                        {QStringLiteral("removedFrames"), removedFrames},
                        {QStringLiteral("baselineFrameCount"), baselineFrames.size()},
                        {QStringLiteral("currentFrameCount"), currentFrames.size()}
                    }}
                });
            }
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
