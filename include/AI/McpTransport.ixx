module;
#include <functional>
#include <QByteArray>
#include <QDeadlineTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVariant>

export module Core.AI.McpTransport;

import std;
import Core.AI.Context;
import Core.AI.McpBridge;
import Core.AI.McpToolMapper;
import Core.AI.ToolExecutor;

export namespace ArtifactCore {

// ─────────────────────────────────────────────
// Extended error codes for better diagnostics
// ─────────────────────────────────────────────
enum class McpErrorCode : int {
    // JSON-RPC standard errors
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    // Transport-level errors
    TransportStartFailed = -40001,
    TransportWriteFailed = -40002,
    TransportWriteTimeout = -40003,
    TransportReadTimeout = -40004,
    ProcessCrashed = -40005,
    EmptyProgram = -40006,

    // Server lifecycle errors
    ServerNotInitialized = -40010,
    ServerDisconnected = -40011,
    RestartLimitExceeded = -40012,
};

struct McpCallResult {
    bool success = false;
    QJsonObject response;
    QString errorText;
    int errorCode = 0;  // McpErrorCode or process error code

    bool hasError() const { return !success; }
};

// Progress notification from MCP server (progressToken)
struct McpProgressInfo {
    QString token;           // progressToken value
    int current = 0;         // current progress (0-based)
    int total = 0;           // total expected (0 if unknown)
    QString message;         // optional human-readable status
};

using McpProgressCallback = std::function<void(const McpProgressInfo&)>;

// Utility: check if a decoded frame is a progress notification and invoke callback
inline bool tryHandleProgressFrame(const QJsonObject& frame, const McpProgressCallback& onProgress) {
    const QString method = frame.value(QStringLiteral("method")).toString();
    if (method == QStringLiteral("progress") || method.startsWith(QStringLiteral("notifications/progress"))) {
        if (onProgress) {
            const QJsonObject params = frame.value(QStringLiteral("params")).toObject();
            McpProgressInfo info;
            info.token = params.value(QStringLiteral("progressToken")).toString();
            info.current = params.value(QStringLiteral("progress")).toInt();
            info.total = params.value(QStringLiteral("total")).toInt(0);
            info.message = params.value(QStringLiteral("message")).toString();
            onProgress(info);
        }
        return true;
    }
    return false;
}

class McpTransportSession {
public:
  void setProgram(const QString &program) { program_ = program; }
  void setArguments(const QStringList &arguments) { arguments_ = arguments; }
  void setWorkingDirectory(const QString &workingDirectory) {
    workingDirectory_ = workingDirectory;
  }
  void setEnvironment(const QProcessEnvironment &environment) {
    environment_ = environment;
    hasEnvironment_ = true;
  }

  QString program() const { return program_; }
  QStringList arguments() const { return arguments_; }
  QString workingDirectory() const { return workingDirectory_; }

  bool isRunning() const {
    return process_ && process_->state() != QProcess::NotRunning;
  }

  QString lastError() const { return lastError_; }

  bool start() {
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

  void stop() {
    if (!process_) {
      return;
    }
    if (process_->state() != QProcess::NotRunning) {
      process_->kill();
      process_->waitForFinished(2000);
    }
  }

  // ── Call with progress callback ──

  McpCallResult callWithProgress(const QJsonObject &request,
                                 McpProgressCallback onProgress,
                                 const AIContext &context = AIContext(),
                                 int timeoutMs = 15000) {
    if (!process_ || process_->state() == QProcess::NotRunning) {
      if (!start()) {
        return {false, QJsonObject{},
                lastError_.isEmpty()
                    ? QStringLiteral("Failed to start MCP transport")
                    : lastError_,
                static_cast<int>(McpErrorCode::TransportStartFailed)};
      }
    }

    // Inject progressToken into _meta if callback is provided
    QJsonObject requestWithContext = request;
    QJsonObject params =
        requestWithContext.value(QStringLiteral("params")).toObject();
    params[QStringLiteral("context")] = context.toJson();

    if (onProgress) {
      const QString progressToken = QStringLiteral("progress_") + QString::number(++progressTokenCounter_);
      QJsonObject meta = params.value(QStringLiteral("_meta")).toObject();
      meta[QStringLiteral("progressToken")] = progressToken;
      params[QStringLiteral("_meta")] = meta;
    }
    requestWithContext[QStringLiteral("params")] = params;

    const QByteArray frame = McpBridge::encodeFrame(requestWithContext);
    if (process_->write(frame) < 0) {
      lastError_ = process_->errorString();
      return {false, QJsonObject{}, lastError_,
              static_cast<int>(McpErrorCode::TransportWriteFailed)};
    }
    if (!process_->waitForBytesWritten(timeoutMs)) {
      lastError_ = QStringLiteral("Timed out writing MCP request");
      return {false, QJsonObject{}, lastError_,
              static_cast<int>(McpErrorCode::TransportWriteTimeout)};
    }

    return waitForResponseWithProgress(timeoutMs, onProgress);
  }

  // ── Simple call (no progress) ──

  McpCallResult call(const QJsonObject &request,
                     const AIContext &context = AIContext(),
                     int timeoutMs = 15000) {
    return callWithProgress(request, McpProgressCallback(), context, timeoutMs);
  }

  McpCallResult initialize(const AIContext &context = AIContext()) {
    QJsonObject request{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 1},
        {QStringLiteral("method"), QStringLiteral("initialize")},
        {QStringLiteral("params"),
         QJsonObject{
             {QStringLiteral("clientInfo"),
              QJsonObject{
                  {QStringLiteral("name"), QStringLiteral("ArtifactStudio")},
                  {QStringLiteral("version"), QStringLiteral("0.9.0")}}}}}};
    return call(request, context);
  }

  McpCallResult listTools(const AIContext &context = AIContext()) {
    QJsonObject request{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 2},
        {QStringLiteral("method"), QStringLiteral("tools/list")},
        {QStringLiteral("params"), QJsonObject{}}};
    return call(request, context);
  }

  McpCallResult callTool(const QJsonObject &toolCall,
                         const AIContext &context = AIContext()) {
    QJsonObject request{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 3},
        {QStringLiteral("method"), QStringLiteral("tools/call")},
        {QStringLiteral("params"),
         QJsonObject{{QStringLiteral("tool"), toolCall}}}};
    return call(request, context);
  }

  QStringList availableToolNames(const AIContext &context = AIContext()) {
    const auto result = listTools(context);
    if (!result.success ||
        !result.response.contains(QStringLiteral("result"))) {
      return {};
    }
    const QJsonObject resultObj =
        result.response.value(QStringLiteral("result")).toObject();
    const QJsonArray tools = resultObj.value(QStringLiteral("tools")).toArray();
    QStringList names;
    for (const auto &value : tools) {
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
  int progressTokenCounter_ = 0;

  McpCallResult waitForResponseWithProgress(int timeoutMs, McpProgressCallback onProgress) {
    QByteArray stdoutBytes;
    QByteArray stderrBytes;
    const auto deadline = QDeadlineTimer(timeoutMs);

    while (deadline.remainingTime() > 0) {
      if (process_->waitForReadyRead(50)) {
        stdoutBytes += process_->readAllStandardOutput();
        stderrBytes += process_->readAllStandardError();

        // Decode all available frames from the buffer
        responseBuffer_ += stdoutBytes;
        stdoutBytes.clear();

        const auto frames = McpBridge::decodeFrames(responseBuffer_);
        if (!frames.isEmpty()) {
          // Find the position after the last complete frame to update buffer
          const int lastFrameEnd = responseBuffer_.lastIndexOf("\r\n\r\n");
          if (lastFrameEnd >= 0) {
            // Find the body end of the last frame
            const QByteArray header = responseBuffer_.left(lastFrameEnd);
            const int clPos = header.toLower().lastIndexOf("content-length:");
            if (clPos >= 0) {
              const int clStart = clPos + 15;
              const int clEnd = header.indexOf('\r', clStart);
              if (clEnd > clStart) {
                bool ok = false;
                const int contentLength = header.mid(clStart, clEnd - clStart).trimmed().toInt(&ok);
                if (ok && contentLength >= 0) {
                  const int bodyEnd = lastFrameEnd + 4 + contentLength;
                  if (bodyEnd <= responseBuffer_.size()) {
                    responseBuffer_ = responseBuffer_.mid(bodyEnd);
                  }
                }
              }
            }
          }

          // Process each frame - filter progress notifications vs final responses
          for (const auto& frame : frames) {
            if (tryHandleProgressFrame(frame, onProgress)) {
              continue;
            }
            lastError_.clear();
            return {true, frame, QString(), 0};
          }
        }
      }
      if (process_->state() == QProcess::NotRunning) {
        break;
      }
    }

    stderrBytes += process_->readAllStandardError();
    const bool crashed = (process_->state() == QProcess::NotRunning &&
                          process_->exitStatus() == QProcess::CrashExit);
    lastError_ = stderrBytes.isEmpty()
                     ? (crashed ? QStringLiteral("MCP process crashed")
                                : QStringLiteral("Timed out waiting for MCP response"))
                     : QString::fromUtf8(stderrBytes).trimmed();
    return {false, QJsonObject{}, lastError_,
            crashed ? static_cast<int>(McpErrorCode::ProcessCrashed)
                    : static_cast<int>(McpErrorCode::TransportReadTimeout)};
  }

};

} // namespace ArtifactCore
