module;
#include <QByteArray>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <map>
#include <utility>
#include <memory>

export module Core.AI.McpServerManager;

import Core.AI.Context;
import Core.AI.McpBridge;
import Core.AI.McpTransport;

export namespace ArtifactCore {

// ─────────────────────────────────────────────
// Server state
// ─────────────────────────────────────────────
enum class McpServerState : int {
    Idle = 0,
    Starting = 1,
    Initializing = 2,
    Ready = 3,
    Error = 4,
    Stopping = 5,
    Stopped = 6,
};

inline QString toString(McpServerState state)
{
    switch (state) {
    case McpServerState::Idle:         return QStringLiteral("Idle");
    case McpServerState::Starting:     return QStringLiteral("Starting");
    case McpServerState::Initializing: return QStringLiteral("Initializing");
    case McpServerState::Ready:        return QStringLiteral("Ready");
    case McpServerState::Error:        return QStringLiteral("Error");
    case McpServerState::Stopping:     return QStringLiteral("Stopping");
    case McpServerState::Stopped:      return QStringLiteral("Stopped");
    }
    return QStringLiteral("Unknown");
}

// ─────────────────────────────────────────────
// Per-server configuration & runtime data
// ─────────────────────────────────────────────
struct McpServerConfig {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment;
    bool hasEnvironment = false;

    int restartLimit = 3;            // 自動再起動の最大リトライ数
    int restartWindowMs = 30000;     // リトライウィンドウ（ミリ秒）
    int startTimeoutMs = 10000;      // 起動待機タイムアウト
    int callTimeoutMs = 15000;       // ツール呼び出しタイムアウト
};

struct McpServerError {
    QString message;
    int code = -1;
};

// McpCallResult and McpProgressCallback are now imported from Core.AI.McpTransport

struct McpServerInstance {
    McpServerConfig config;
    McpServerState state = McpServerState::Idle;
    McpServerError lastError;

    std::unique_ptr<QProcess> process;
    QByteArray readBuffer;           // 未処理の受信データ蓄積用
    int restartCount = 0;
    qint64 firstRestartTime = 0;     // リトライウィンドウ計測用

    QStringList availableTools;      // initialize後に設定されるツール一覧
};

// ─────────────────────────────────────────────
// McpServerManager
// ─────────────────────────────────────────────
class McpServerManager {
public:
    static McpServerManager& instance() {
        static McpServerManager mgr;
        return mgr;
    }

    // ── サーバー登録・削除 ──

    bool registerServer(const QString& id, const McpServerConfig& config)
    {
        if (id.trimmed().isEmpty()) {
            lastGlobalError_ = QStringLiteral("Server ID is empty");
            return false;
        }
        if (config.program.trimmed().isEmpty()) {
            lastGlobalError_ = QStringLiteral("Server program is empty for: ") + id;
            return false;
        }
        if (servers_.contains(id)) {
            lastGlobalError_ = QStringLiteral("Server already registered: ") + id;
            return false;
        }
        McpServerInstance inst;
        inst.config = config;
        inst.state = McpServerState::Idle;
        servers_[id] = std::move(inst);
        return true;
    }

    bool registerServer(const QString& id, const QString& program,
                        const QStringList& args = {},
                        const QString& workDir = {})
    {
        McpServerConfig config;
        config.program = program;
        config.arguments = args;
        config.workingDirectory = workDir;
        return registerServer(id, config);
    }

    void removeServer(const QString& id)
    {
        auto it = servers_.find(id);
        if (it != servers_.end()) {
            McpServerInstance& srv = it->second;
            if (srv.process &&
                srv.process->state() != QProcess::NotRunning) {
                srv.process->kill();
                srv.process->waitForFinished(2000);
            }
            servers_.erase(it);
        }
    }

    // ── ライフサイクル ──

    bool startServer(const QString& id)
    {
        auto it = servers_.find(id);
        if (it == servers_.end()) {
            lastGlobalError_ = QStringLiteral("Server not found: ") + id;
            return false;
        }

        McpServerInstance& srv = it->second;

        if (srv.state == McpServerState::Ready) {
            return true; // すでに起動済み
        }
        if (srv.state == McpServerState::Starting ||
            srv.state == McpServerState::Initializing) {
            lastGlobalError_ = QStringLiteral("Server is already starting: ") + id;
            return false;
        }

        srv.state = McpServerState::Starting;
        srv.lastError = {};

        if (!ensureProcessCreated(srv)) {
            srv.state = McpServerState::Error;
            return false;
        }

        if (!srv.config.workingDirectory.trimmed().isEmpty()) {
            srv.process->setWorkingDirectory(srv.config.workingDirectory);
        }
        if (srv.config.hasEnvironment) {
            srv.process->setProcessEnvironment(srv.config.environment);
        }
        srv.process->setProgram(srv.config.program);
        srv.process->setArguments(srv.config.arguments);
        srv.process->setProcessChannelMode(QProcess::SeparateChannels);

        srv.process->start();
        if (!srv.process->waitForStarted(srv.config.startTimeoutMs)) {
            srv.lastError.message = srv.process->errorString();
            srv.lastError.code = static_cast<int>(srv.process->error());
            srv.state = McpServerState::Error;
            return false;
        }

        // initialize ハンドシェイク
        srv.state = McpServerState::Initializing;
        QJsonObject initClientInfo;
        initClientInfo[QStringLiteral("name")] = QStringLiteral("ArtifactStudio");
        initClientInfo[QStringLiteral("version")] = QStringLiteral("0.9.0");

        QJsonObject initParams;
        initParams[QStringLiteral("protocolVersion")] = QStringLiteral("2024-11-05");
        initParams[QStringLiteral("clientInfo")] = initClientInfo;

        QJsonObject initRequest;
        initRequest[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        initRequest[QStringLiteral("id")] = 1;
        initRequest[QStringLiteral("method")] = QStringLiteral("initialize");
        initRequest[QStringLiteral("params")] = initParams;

        const McpCallResult initResult =
            callRaw(id, initRequest, AIContext(), srv.config.startTimeoutMs);

        if (!initResult.success) {
            srv.lastError.message = initResult.errorText;
            srv.lastError.code = -1;
            srv.state = McpServerState::Error;
            stopProcess(srv);
            return false;
        }

        // tools/list で利用可能ツールを取得
        refreshAvailableTools(id);

        srv.state = McpServerState::Ready;
        srv.restartCount = 0;
        srv.firstRestartTime = 0;
        return true;
    }

    void stopServer(const QString& id)
    {
        auto it = servers_.find(id);
        if (it == servers_.end()) {
            return;
        }
        McpServerInstance& srv = it->second;
        srv.state = McpServerState::Stopping;
        stopProcess(srv);
        srv.state = McpServerState::Stopped;
        srv.readBuffer.clear();
    }

    void stopAll()
    {
        for (const auto& pair : servers_) {
            stopServer(pair.first);
        }
    }

    // ── 状態取得 ──

    McpServerState serverState(const QString& id) const
    {
        auto it = servers_.find(id);
        if (it == servers_.end()) {
            return McpServerState::Error;
        }
        return it->second.state;
    }

    McpServerError serverLastError(const QString& id) const
    {
        auto it = servers_.find(id);
        if (it == servers_.end()) {
            return {QStringLiteral("Server not found: ") + id, -1};
        }
        return it->second.lastError;
    }

    QStringList serverIds() const
    {
        QStringList ids;
        for (const auto& pair : servers_) {
            ids.push_back(pair.first);
        }
        return ids;
    }

    QStringList serverToolNames(const QString& id) const
    {
        auto it = servers_.find(id);
        if (it == servers_.end()) {
            return {};
        }
        return it->second.availableTools;
    }

    QString lastGlobalError() const { return lastGlobalError_; }

    // ── ツール呼び出し ──

    // プログレス対応版
    McpCallResult callToolWithProgress(const QString& serverId,
                                       const QJsonObject& toolCall,
                                       McpProgressCallback onProgress,
                                       const AIContext& context = AIContext())
    {
        auto it = servers_.find(serverId);
        if (it == servers_.end()) {
            return {false, QJsonObject{}, QStringLiteral("Server not found: ") + serverId,
                    static_cast<int>(McpErrorCode::ServerDisconnected)};
        }

        McpServerInstance& srv = it->second;

        // Ready でなければ起動を試みる
        if (srv.state != McpServerState::Ready) {
            if (!startServer(serverId)) {
                return {false, QJsonObject{}, srv.lastError.message,
                        static_cast<int>(McpErrorCode::ServerNotInitialized)};
            }
        }

        // プロセスが生きていない場合は再起動
        if (!srv.process || srv.process->state() == QProcess::NotRunning) {
            if (!tryRestartIfNeeded(srv, serverId)) {
                return {false, QJsonObject{}, srv.lastError.message,
                        static_cast<int>(McpErrorCode::ProcessCrashed)};
            }
        }

        QJsonObject request;
        request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        request[QStringLiteral("id")] = 3;
        request[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("tool")] = toolCall;
        request[QStringLiteral("params")] = params;

        return callRawWithProgress(serverId, request, std::move(onProgress), context, srv.config.callTimeoutMs);
    }

    // 旧シグネチャ（後方互換）
    McpCallResult callTool(const QString& serverId,
                           const QJsonObject& toolCall,
                           const AIContext& context = AIContext())
    {
        return callToolWithProgress(serverId, toolCall, McpProgressCallback(), context);
    }

    // MCPサーバーに直接リクエストを送信（汎用・プログレス対応）
    McpCallResult callRawWithProgress(const QString& serverId,
                                      const QJsonObject& request,
                                      McpProgressCallback onProgress,
                                      const AIContext& context = AIContext(),
                                      int timeoutMs = 15000)
    {
        auto it = servers_.find(serverId);
        if (it == servers_.end()) {
            return {false, QJsonObject{}, QStringLiteral("Server not found: ") + serverId,
                    static_cast<int>(McpErrorCode::ServerDisconnected)};
        }

        McpServerInstance& srv = it->second;
        if (!srv.process || srv.process->state() == QProcess::NotRunning) {
            return {false, QJsonObject{},
                    srv.lastError.message.isEmpty()
                        ? QStringLiteral("Server process is not running")
                        : srv.lastError.message,
                    static_cast<int>(McpErrorCode::ServerDisconnected)};
        }

        // Inject progressToken if callback provided
        QJsonObject requestWithContext = request;
        QJsonObject params = requestWithContext.value(QStringLiteral("params")).toObject();
        params[QStringLiteral("context")] = context.toJson();

        if (onProgress) {
            const QString progressToken = QStringLiteral("srv_progress_") + QString::number(++srvProgressTokenCounter_);
            QJsonObject meta = params.value(QStringLiteral("_meta")).toObject();
            meta[QStringLiteral("progressToken")] = progressToken;
            params[QStringLiteral("_meta")] = meta;
        }
        requestWithContext[QStringLiteral("params")] = params;

        const QByteArray frame = McpBridge::encodeFrame(requestWithContext);
        if (srv.process->write(frame) < 0) {
            srv.lastError.message = srv.process->errorString();
            srv.lastError.code = static_cast<int>(srv.process->error());
            return {false, QJsonObject{}, srv.lastError.message,
                    static_cast<int>(McpErrorCode::TransportWriteFailed)};
        }
        if (!srv.process->waitForBytesWritten(timeoutMs)) {
            srv.lastError.message = QStringLiteral("Timed out writing request");
            return {false, QJsonObject{}, srv.lastError.message,
                    static_cast<int>(McpErrorCode::TransportWriteTimeout)};
        }

        return waitForResponseWithProgress(srv, timeoutMs, std::move(onProgress));
    }

    // 旧シグネチャ（後方互換）
    McpCallResult callRaw(const QString& serverId,
                          const QJsonObject& request,
                          const AIContext& context = AIContext(),
                          int timeoutMs = 15000)
    {
        return callRawWithProgress(serverId, request, McpProgressCallback(), context, timeoutMs);
    }

    // ── 自動再起動 ──

    void setRestartLimit(const QString& serverId, int limit, int windowMs = 30000)
    {
        auto it = servers_.find(serverId);
        if (it != servers_.end()) {
            it->second.config.restartLimit = limit;
            it->second.config.restartWindowMs = windowMs;
        }
    }

private:
    std::map<QString, McpServerInstance> servers_;
    QString lastGlobalError_;
    int srvProgressTokenCounter_ = 0;

    McpServerManager() = default;
    ~McpServerManager() { stopAll(); }

    // ── 内部ヘルパー ──

    bool ensureProcessCreated(McpServerInstance& srv)
    {
        if (!srv.process) {
            srv.process = std::make_unique<QProcess>();
            QObject::connect(srv.process.get(), &QProcess::finished,
                             [&srv](int exitCode, QProcess::ExitStatus exitStatus) {
                if (srv.state == McpServerState::Stopping ||
                    srv.state == McpServerState::Stopped) {
                    return;
                }
                srv.state = McpServerState::Error;
                if (exitStatus == QProcess::CrashExit) {
                    srv.lastError.message = QStringLiteral("MCP server process crashed");
                    srv.lastError.code = -1;
                } else {
                    srv.lastError.message = QStringLiteral("MCP server process exited with code: ")
                                            + QString::number(exitCode);
                    srv.lastError.code = exitCode;
                }
            });
        }
        return true;
    }

    void stopProcess(McpServerInstance& srv)
    {
        if (!srv.process) {
            return;
        }
        if (srv.process->state() != QProcess::NotRunning) {
            srv.process->kill();
            srv.process->waitForFinished(2000);
        }
    }

    bool refreshAvailableTools(const QString& id)
    {
        auto it = servers_.find(id);
        if (it == servers_.end()) {
            return false;
        }
        McpServerInstance& srv = it->second;

        QJsonObject listRequest;
        listRequest[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        listRequest[QStringLiteral("id")] = 2;
        listRequest[QStringLiteral("method")] = QStringLiteral("tools/list");
        listRequest[QStringLiteral("params")] = QJsonObject{};

        const McpCallResult toolsResult =
            callRaw(id, listRequest, AIContext(), srv.config.callTimeoutMs);

        if (!toolsResult.success) {
            return false;
        }

        srv.availableTools.clear();
        const QJsonObject resultObj =
            toolsResult.response.value(QStringLiteral("result")).toObject();
        const QJsonArray tools = resultObj.value(QStringLiteral("tools")).toArray();
        for (const auto& value : tools) {
            const QJsonObject tool = value.toObject();
            const QString name = tool.value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) {
                srv.availableTools.push_back(name);
            }
        }
        return true;
    }

    McpCallResult waitForResponseWithProgress(McpServerInstance& srv, int timeoutMs, McpProgressCallback onProgress)
    {
        QByteArray stdoutBytes;
        QByteArray stderrBytes;
        const auto deadline = QDeadlineTimer(timeoutMs);

        while (deadline.remainingTime() > 0) {
            if (srv.process->waitForReadyRead(50)) {
                stdoutBytes += srv.process->readAllStandardOutput();
                stderrBytes += srv.process->readAllStandardError();

                srv.readBuffer += stdoutBytes;
                stdoutBytes.clear();

                // Decode all available frames
                const auto frames = McpBridge::decodeFrames(srv.readBuffer);
                if (!frames.isEmpty()) {
                    // Advance buffer past the last complete frame
                    if (!frames.isEmpty()) {
                        const QByteArray body = QJsonDocument(frames.last()).toJson(QJsonDocument::Compact);
                        const int headerEnd = srv.readBuffer.indexOf("\r\n\r\n");
                        if (headerEnd >= 0) {
                            const QByteArray header = srv.readBuffer.left(headerEnd);
                            const int clPos = header.toLower().lastIndexOf("content-length:");
                            if (clPos >= 0) {
                                bool ok = false;
                                const int contentLength = header.mid(clPos + 15, header.indexOf('\r', clPos) - clPos - 15).trimmed().toInt(&ok);
                                if (ok) {
                                    // Find and skip all complete frames
                                    int offset = 0;
                                    for (const auto& frame : frames) {
                                        const int he = srv.readBuffer.indexOf("\r\n\r\n", offset);
                                        if (he < 0) break;
                                        const QByteArray h = srv.readBuffer.mid(offset, he - offset);
                                        const int cp = h.toLower().lastIndexOf("content-length:");
                                        if (cp >= 0) {
                                            bool ok2 = false;
                                            const int cl = h.mid(cp + 15, h.indexOf('\r', cp) - cp - 15).trimmed().toInt(&ok2);
                                            if (ok2) {
                                                offset = he + 4 + cl;
                                            } else {
                                                break;
                                            }
                                        } else {
                                            break;
                                        }
                                    }
                                    if (offset > 0) {
                                        srv.readBuffer = srv.readBuffer.mid(offset);
                                    }
                                }
                            }
                        }
                    }

                    // Process each frame
                    for (const auto& frame : frames) {
                        if (tryHandleProgressFrame(frame, onProgress)) {
                            continue;
                        }
                        srv.lastError.message.clear();
                        return {true, frame, QString(), 0};
                    }
                }
            }
            if (srv.process->state() == QProcess::NotRunning) {
                break;
            }
        }

        stderrBytes += srv.process->readAllStandardError();
        const bool crashed = (srv.process->state() == QProcess::NotRunning &&
                              srv.process->exitStatus() == QProcess::CrashExit);
        srv.lastError.message = stderrBytes.isEmpty()
                                    ? (crashed ? QStringLiteral("Server process crashed")
                                               : QStringLiteral("Timed out waiting for response"))
                                    : QString::fromUtf8(stderrBytes).trimmed();
        return {false, QJsonObject{}, srv.lastError.message,
                crashed ? static_cast<int>(McpErrorCode::ProcessCrashed)
                        : static_cast<int>(McpErrorCode::TransportReadTimeout)};
    }

    bool tryRestartIfNeeded(McpServerInstance& srv, const QString& serverId)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        // リトライウィンドウ外ならカウンタリセット
        if (srv.firstRestartTime == 0 ||
            (now - srv.firstRestartTime) > srv.config.restartWindowMs) {
            srv.restartCount = 0;
            srv.firstRestartTime = now;
        }

        if (srv.restartCount >= srv.config.restartLimit) {
            srv.lastError.message = QStringLiteral("Restart limit reached for server: ") + serverId;
            srv.state = McpServerState::Error;
            return false;
        }

        srv.restartCount++;
        srv.readBuffer.clear();

        if (!startServer(serverId)) {
            return false;
        }

        return srv.state == McpServerState::Ready;
    }
};

} // namespace ArtifactCore
