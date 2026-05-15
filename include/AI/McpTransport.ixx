module;
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDeadlineTimer>

export module Core.AI.McpTransport;

import std;
import Core.AI.Context;
import Core.AI.McpBridge;

export namespace ArtifactCore {

struct McpCallResult {
    bool success = false;
    QJsonObject response;
    QString errorText;
};

class McpTransportSession {
public:
    void setProgram(const QString& program) { program_ = program; }
    void setArguments(const QStringList& arguments) { arguments_ = arguments; }
    void setWorkingDirectory(const QString& workingDirectory) { workingDirectory_ = workingDirectory; }
    void setEnvironment(const QProcessEnvironment& environment) { environment_ = environment; hasEnvironment_ = true; }

    QString program() const { return program_; }
    QStringList arguments() const { return arguments_; }
    QString workingDirectory() const { return workingDirectory_; }

    bool isRunning() const
    {
        return process_ && process_->state() != QProcess::NotRunning;
    }

    QString lastError() const { return lastError_; }

    bool start()
    {
        if (program_.trimmed().isEmpty()) {
            lastError_ = QStringLiteral("MCP transport program is empty");
            return false;
        }
        if (!process_) {
            process_ = std::make_unique<QProcess>();
            process_->setProcessChannelMode(QProcess::SeparateChannels);
        }
        if (!workingDirectory_.trimmed().isEmpty()) {
            process_->setWorkingDirectory(workingDirectory_);
        }
        if (hasEnvironment_) {
            process_->setProcessEnvironment(environment_);
        }
        process_->setProgram(program_);
        process_->setArguments(arguments_);
        process_->start();
        if (!process_->waitForStarted(10000)) {
            lastError_ = process_->errorString();
            return false;
        }
        return true;
    }

    void stop()
    {
        if (!process_) {
            return;
        }
        if (process_->state() != QProcess::NotRunning) {
            process_->kill();
            process_->waitForFinished(2000);
        }
    }

    McpCallResult call(const QJsonObject& request, const AIContext& context = AIContext(), int timeoutMs = 15000)
    {
        if (!process_ || process_->state() == QProcess::NotRunning) {
            if (!start()) {
                return {false, QJsonObject{}, lastError_.isEmpty() ? QStringLiteral("Failed to start MCP transport") : lastError_};
            }
        }

        QJsonObject requestWithContext = request;
        QJsonObject params = requestWithContext.value(QStringLiteral("params")).toObject();
        params[QStringLiteral("context")] = context.toJson();
        requestWithContext[QStringLiteral("params")] = params;

        const QByteArray frame = McpBridge::encodeFrame(requestWithContext);
        if (process_->write(frame) < 0) {
            lastError_ = process_->errorString();
            return {false, QJsonObject{}, lastError_};
        }
        if (!process_->waitForBytesWritten(timeoutMs)) {
            lastError_ = QStringLiteral("Timed out writing MCP request");
            return {false, QJsonObject{}, lastError_};
        }

        QByteArray stdoutBytes;
        QByteArray stderrBytes;
        const auto deadline = QDeadlineTimer(timeoutMs);
        while (deadline.remainingTime() > 0) {
            if (process_->waitForReadyRead(50)) {
                stdoutBytes += process_->readAllStandardOutput();
                stderrBytes += process_->readAllStandardError();
                const auto frames = McpBridge::decodeFrames(stdoutBytes);
                if (!frames.isEmpty()) {
                    responseBuffer_ = stdoutBytes.mid(stdoutBytes.lastIndexOf("\r\n\r\n") + 4);
                    lastError_.clear();
                    return {true, frames.last(), QString()};
                }
            }
            if (process_->state() == QProcess::NotRunning) {
                break;
            }
        }

        stderrBytes += process_->readAllStandardError();
        lastError_ = stderrBytes.isEmpty() ? QStringLiteral("Timed out waiting for MCP response")
                                           : QString::fromUtf8(stderrBytes).trimmed();
        return {false, QJsonObject{}, lastError_};
    }

    McpCallResult initialize(const AIContext& context = AIContext())
    {
        QJsonObject request{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 1},
            {QStringLiteral("method"), QStringLiteral("initialize")},
            {QStringLiteral("params"), QJsonObject{
                {QStringLiteral("clientInfo"), QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("ArtifactStudio")},
                    {QStringLiteral("version"), QStringLiteral("0.9.0")}
                }}
            }}
        };
        return call(request, context);
    }

    McpCallResult listTools(const AIContext& context = AIContext())
    {
        QJsonObject request{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 2},
            {QStringLiteral("method"), QStringLiteral("tools/list")},
            {QStringLiteral("params"), QJsonObject{}}
        };
        return call(request, context);
    }

    McpCallResult callTool(const QJsonObject& toolCall, const AIContext& context = AIContext())
    {
        QJsonObject request{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 3},
            {QStringLiteral("method"), QStringLiteral("tools/call")},
            {QStringLiteral("params"), QJsonObject{
                {QStringLiteral("tool"), toolCall}
            }}
        };
        return call(request, context);
    }

    QStringList availableToolNames(const AIContext& context = AIContext())
    {
        const auto result = listTools(context);
        if (!result.success || !result.response.contains(QStringLiteral("result"))) {
            return {};
        }
        const QJsonObject resultObj = result.response.value(QStringLiteral("result")).toObject();
        const QJsonArray tools = resultObj.value(QStringLiteral("tools")).toArray();
        QStringList names;
        for (const auto& value : tools) {
            const QJsonObject tool = value.toObject();
            const QString name = tool.value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) {
                names.push_back(name);
            }
        }
        return names;
    }

private:
    std::unique_ptr<QProcess> process_;
    QString program_;
    QStringList arguments_;
    QString workingDirectory_;
    QProcessEnvironment environment_;
    bool hasEnvironment_ = false;
    QByteArray responseBuffer_;
    QString lastError_;
};

} // namespace ArtifactCore
