module;
#include <utility>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>
#include <functional>
#include <QString>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QFile>
#include <QDateTime>
#include <QUrl>
#include <QDebug>
#include <cstdint>

module NetworkRPCServer;
import NetworkRPCServer;

namespace ArtifactCore {

class FarmTcpServer final : public QTcpServer {
public:
    QSslCertificate certificate;
    QSslKey privateKey;
    bool tlsEnabled = false;

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        if (!tlsEnabled) {
            QTcpServer::incomingConnection(socketDescriptor);
            return;
        }
        auto* socket = new QSslSocket(this);
        socket->setLocalCertificate(certificate);
        socket->setPrivateKey(privateKey);
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            socket->deleteLater();
            return;
        }
        socket->startServerEncryption();
        addPendingConnection(socket);
        Q_EMIT newConnection();
    }
};

class NetworkPCServer::Impl {
public:
    FarmTcpServer* server_ = nullptr;
    FarmTcpServer* httpServer_ = nullptr;
    unsigned short port_ = 0;
    unsigned short httpPort_ = 0;
    bool httpConnectionsSetup_ = false;
    bool running_ = false;

    std::map<QTcpSocket*, RemoteWorkerInfo> workers_;
    std::map<QString, QTcpSocket*> workerSockets_;
    std::map<QTcpSocket*, QByteArray> readBuffers_;
    QJsonArray workerLogs_;
    mutable std::mutex mutex_;

    WorkerConnectedCallback onWorkerConnected_;
    WorkerDisconnectedCallback onWorkerDisconnected_;
    WorkerHeartbeatCallback onWorkerHeartbeat_;
    RpcRequestHandler onRequest_;
    HttpStatusProvider httpStatusProvider_;
    QString authToken_;
    std::uint64_t nextRpcId_ = 1;
    QSslCertificate tlsCertificate_;
    QSslKey tlsPrivateKey_;
    bool tlsEnabled_ = false;

    // Heartbeat: timer-based dead detection via QObject::connect + singleShot chain
    static constexpr qint64 HEARTBEAT_TIMEOUT_MS = 30000;
    static constexpr qint64 HEARTBEAT_CHECK_INTERVAL_MS = 5000;

    Impl() {
        server_ = new FarmTcpServer();
        httpServer_ = new QTcpServer();
    }

    ~Impl() {
        stopInternal();
        stopHttpApi();
        delete server_;
        delete httpServer_;
    }

    void setupConnections() {
        QTcpServer* srv = server_;
        QObject::connect(srv, &QTcpServer::newConnection, [this, srv]() {
            while (srv->hasPendingConnections()) {
                QTcpSocket* socket = srv->nextPendingConnection();
                onNewConnection(socket);
            }
        });
    }

    void scheduleHeartbeatCheck() {
        QTimer::singleShot(HEARTBEAT_CHECK_INTERVAL_MS, [this]() {
            if (!running_) return;
            checkHeartbeats();
            scheduleHeartbeatCheck();
        });
    }

    void onNewConnection(QTcpSocket* socket) {
        RemoteWorkerInfo info;
        info.address = socket->peerAddress().toString();
        info.port = socket->peerPort();
        info.connected = true;
        info.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            workers_[socket] = info;
            readBuffers_[socket] = {};
        }

        QObject::connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            onData(socket);
        });

        QObject::connect(socket, &QTcpSocket::disconnected, [this, socket]() {
            onDisconnect(socket);
        });

        qDebug() << "[Farm] TCP connected:" << info.address;
    }

    void onData(QTcpSocket* socket) {
        auto& buffer = readBuffers_[socket];
        buffer.append(socket->readAll());
        constexpr qsizetype kMaxRpcMessageBytes = 16 * 1024 * 1024;
        if (buffer.size() > kMaxRpcMessageBytes) {
            qWarning() << "[Farm] RPC message exceeds size limit:" << socket->peerAddress().toString();
            socket->disconnectFromHost();
            return;
        }
        while (true) {
            const qsizetype newline = buffer.indexOf('\n');
            if (newline < 0) break;
            const QByteArray line = buffer.left(newline).trimmed();
            buffer.remove(0, newline + 1);
            if (line.isEmpty()) continue;
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) continue;
            const QJsonObject msg = doc.object();
            const QString method = msg["method"].toString();
            if (method == "register") {
                handleRegister(socket, msg);
            } else if (method == "heartbeat") {
                handleHeartbeat(socket, msg);
            } else {
                handleRpc(socket, msg);
            }
        }
    }

    void handleRegister(QTcpSocket* socket, const QJsonObject& msg) {
        const QJsonObject params = msg["params"].toObject();
        if (!authToken_.isEmpty() && params["authToken"].toString() != authToken_) {
            qWarning() << "[Farm] Worker registration rejected: authentication failed"
                       << socket->peerAddress().toString();
            socket->disconnectFromHost();
            return;
        }
        QString workerId = params["workerId"].toString();
        if (workerId.isEmpty()) {
            qWarning() << "[Farm] Worker registration rejected: missing worker id"
                       << socket->peerAddress().toString();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto existingSocket = workerSockets_.find(workerId);
            if (existingSocket != workerSockets_.end() && existingSocket->second != socket) {
                qWarning() << "[Farm] Replacing stale worker connection:" << workerId;
                readBuffers_.erase(existingSocket->second);
                workers_.erase(existingSocket->second);
                existingSocket->second->disconnectFromHost();
                workerSockets_.erase(existingSocket);
            }
            auto it = workers_.find(socket);
            if (it != workers_.end()) {
                it->second.workerId = workerId;
                it->second.capabilities = msg["params"].toObject()["capabilities"].toObject();
                it->second.state = it->second.capabilities.value(QStringLiteral("maintenance")).toBool(false)
                    ? QStringLiteral("Maintenance") : QStringLiteral("Idle");
                workerSockets_[workerId] = socket;
                it->second.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
            }
        }

        QJsonObject resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = msg["id"];
        resp["result"] = QJsonObject{{"status", "registered"}};
        sendJson(socket, resp);

        if (onWorkerConnected_) {
            onWorkerConnected_(NetworkPCServer::instance().workerInfo(workerId));
        }
    }

    void handleHeartbeat(QTcpSocket* socket, const QJsonObject& msg) {
        QString workerId = msg["params"].toObject()["workerId"].toString();
        if (!workerId.isEmpty()) {
            bool registered = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = workerSockets_.find(workerId);
                if (it != workerSockets_.end() && it->second == socket) {
                    registered = true;
                    auto wit = workers_.find(it->second);
                    if (wit != workers_.end())
                        wit->second.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
                }
            }
            if (registered && onWorkerHeartbeat_)
                onWorkerHeartbeat_(workerId);
        }
    }

    void handleRpc(QTcpSocket* socket, const QJsonObject& msg) {
        QString method = msg["method"].toString();
        QJsonObject params = msg["params"].toObject();
        const bool workerScopedRequest = method == QStringLiteral("workerProgress")
            || method == QStringLiteral("frameCompleted")
            || method == QStringLiteral("frameFailed")
            || method == QStringLiteral("workerLog");
        bool workerIdentityValid = true;
        if (workerScopedRequest) {
            const QString workerId = params[QStringLiteral("workerId")].toString();
            std::lock_guard<std::mutex> lock(mutex_);
            const auto workerSocket = workerSockets_.find(workerId);
            workerIdentityValid = workerSocket != workerSockets_.end()
                && workerSocket->second == socket;
            if (!workerIdentityValid)
                qWarning() << "[Farm] Ignoring worker-scoped RPC from unregistered socket";
        }
        if (workerIdentityValid && method == QStringLiteral("workerProgress")) {
            const QString workerId = params[QStringLiteral("workerId")].toString();
            std::lock_guard<std::mutex> lock(mutex_);
            const auto socketIt = workerSockets_.find(workerId);
            if (socketIt != workerSockets_.end()) {
                const auto workerIt = workers_.find(socketIt->second);
                if (workerIt != workers_.end()) {
                    workerIt->second.completedFrames = std::max(0, params[QStringLiteral("completedFrames")].toInt());
                    workerIt->second.failedFrames = std::max(0, params[QStringLiteral("failedFrames")].toInt());
                    const qint64 renderTimeMs = std::max<qint64>(
                        0, params[QStringLiteral("renderTimeMs")].toVariant().toLongLong());
                    if (renderTimeMs > workerIt->second.renderTimeMs) {
                        workerIt->second.totalRenderTimeMs +=
                            renderTimeMs - workerIt->second.renderTimeMs;
                    }
                    workerIt->second.renderTimeMs = renderTimeMs;
                    workerIt->second.currentFrame = params[QStringLiteral("currentFrame")].toInt(-1);
                }
            }
        }
        if (workerIdentityValid && method == QStringLiteral("workerLog")) {
            const QString workerId = params[QStringLiteral("workerId")].toString();
            const QString severity = params.value(QStringLiteral("severity")).toString().trimmed();
            const QString message = params.value(QStringLiteral("message")).toString().left(4096);
            const QString normalizedSeverity =
                (severity == QStringLiteral("debug") || severity == QStringLiteral("info")
                 || severity == QStringLiteral("warning") || severity == QStringLiteral("error"))
                ? severity : QStringLiteral("info");
            QJsonObject entry{
                {QStringLiteral("workerId"), workerId},
                {QStringLiteral("severity"), normalizedSeverity},
                {QStringLiteral("message"), message},
                {QStringLiteral("frame"), params.value(QStringLiteral("frame")).toInt(-1)},
                {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
            };
            std::lock_guard<std::mutex> lock(mutex_);
            workerLogs_.append(entry);
            while (workerLogs_.size() > 1000)
                workerLogs_.removeFirst();
        }
        if (workerIdentityValid && (method == QStringLiteral("frameCompleted")
            || method == QStringLiteral("frameFailed"))) {
            const QString workerId = params[QStringLiteral("workerId")].toString();
            std::lock_guard<std::mutex> lock(mutex_);
            const auto socketIt = workerSockets_.find(workerId);
            if (socketIt != workerSockets_.end()) {
                const auto workerIt = workers_.find(socketIt->second);
                if (workerIt != workers_.end()) {
                    workerIt->second.assignedFrames = std::max(
                        0, workerIt->second.assignedFrames - 1);
                    if (method == QStringLiteral("frameFailed")) {
                        workerIt->second.state = QStringLiteral("Error");
                    } else if (workerIt->second.assignedFrames == 0) {
                        workerIt->second.state = QStringLiteral("Idle");
                    }
                }
            }
        }
        QJsonObject result;
        if (!workerIdentityValid) {
            result[QStringLiteral("status")] = QStringLiteral("rejected");
        } else if (onRequest_)
            result = onRequest_(method, params);

        QJsonObject resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = msg["id"];
        resp["result"] = result;
        sendJson(socket, resp);
    }

    void onDisconnect(QTcpSocket* socket) {
        QString workerId;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = workers_.find(socket);
            if (it != workers_.end()) {
                workerId = it->second.workerId;
                workerSockets_.erase(workerId);
                workers_.erase(it);
            }
            readBuffers_.erase(socket);
        }
        if (!workerId.isEmpty() && onWorkerDisconnected_)
            onWorkerDisconnected_(workerId);
        socket->deleteLater();
    }

    void checkHeartbeats() {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        std::vector<QTcpSocket*> dead;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [sock, info] : workers_) {
                if (info.connected && (now - info.lastHeartbeat) > HEARTBEAT_TIMEOUT_MS)
                    dead.push_back(sock);
            }
        }
        for (auto* sock : dead) {
            qDebug() << "[Farm] Heartbeat timeout:" << sock->peerAddress().toString();
            sock->disconnectFromHost();
        }
    }

    void sendJson(QTcpSocket* socket, const QJsonObject& obj) {
        QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
        socket->write(data);
        socket->flush();
    }

    void stopInternal() {
        running_ = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [sock, _] : workers_) {
                sock->disconnectFromHost();
                sock->deleteLater();
            }
            workers_.clear();
            workerSockets_.clear();
            readBuffers_.clear();
        }
        if (server_->isListening())
            server_->close();
        stopHttpApi();
    }

    void stopHttpApi() {
        if (httpServer_ && httpServer_->isListening()) httpServer_->close();
        httpPort_ = 0;
    }

    void setupHttpApi() {
        if (httpConnectionsSetup_) return;
        httpConnectionsSetup_ = true;
        QObject::connect(httpServer_, &QTcpServer::newConnection, [this]() {
            while (httpServer_->hasPendingConnections()) {
                QTcpSocket* socket = httpServer_->nextPendingConnection();
                QTimer::singleShot(10000, socket, [socket]() {
                    if (socket->state() != QAbstractSocket::UnconnectedState)
                        socket->disconnectFromHost();
                });
                auto requestBuffer = std::make_shared<QByteArray>();
                QObject::connect(socket, &QTcpSocket::readyRead, [this, socket, requestBuffer]() {
                    requestBuffer->append(socket->readAll());
                    constexpr qsizetype kMaxHttpRequestBytes = 1024 * 1024;
                    if (requestBuffer->size() > kMaxHttpRequestBytes) {
                        socket->disconnectFromHost();
                        return;
                    }
                    const QByteArray& request = *requestBuffer;
                    const QList<QByteArray> lines = request.split('\n');
                    if (lines.isEmpty()) return;
                    const QList<QByteArray> requestLine = lines.front().trimmed().split(' ');
                    const qsizetype bodyOffset = request.indexOf("\r\n\r\n");
                    if (bodyOffset < 0) return;
                    const QByteArray requestBody = bodyOffset >= 0
                        ? request.mid(bodyOffset + 4).trimmed() : QByteArray();
                    if (bodyOffset >= 0 && requestLine.size() >= 2 && requestLine[0] == "POST") {
                        qsizetype contentLength = 0;
                        for (const auto& line : lines) {
                            const QByteArray prefix = "Content-Length:";
                            if (line.left(prefix.size()).toLower() == prefix.toLower()) {
                                contentLength = line.mid(prefix.size()).trimmed().toLongLong();
                                break;
                            }
                        }
                        if (contentLength > 0 && request.size() - bodyOffset - 4 < contentLength)
                            return;
                    }
                    const QByteArray authPrefix = "Authorization: Bearer ";
                    QByteArray authorization;
                    for (const auto& line : lines) {
                        if (line.left(authPrefix.size()).toLower() == authPrefix.toLower()) {
                            authorization = line.mid(authPrefix.size()).trimmed();
                            break;
                        }
                    }
                    const bool authorized = authToken_.isEmpty()
                        || authorization == authToken_.toUtf8();
                    QJsonObject body;
                    QJsonArray responseArray;
                    bool responseIsArray = false;
                    QByteArray contentType = "application/json";
                    QByteArray metricsPayload;
                    int statusCode = 200;
                    QByteArray statusText = "OK";
                    qint64 totalWorkerRenderTimeMs = 0;
                    if (!authorized) {
                        statusCode = 401;
                        statusText = "Unauthorized";
                        body = {{QStringLiteral("error"), QStringLiteral("unauthorized")}};
                    } else if (requestLine.size() >= 2 && requestLine[0] == "OPTIONS") {
                        statusCode = 204;
                        statusText = "No Content";
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/jobs") {
                        QJsonParseError parseError;
                        const QJsonDocument document = QJsonDocument::fromJson(requestBody, &parseError);
                        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_job_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("submitJob"), document.object());
                            const QString jobStatus = body.value(QStringLiteral("status")).toString();
                            if (jobStatus == QStringLiteral("invalid_request")
                                || jobStatus == QStringLiteral("renderer_not_found")) {
                                statusCode = 400;
                                statusText = "Bad Request";
                            } else if (jobStatus == QStringLiteral("busy")) {
                                statusCode = 409;
                                statusText = "Conflict";
                            } else if (jobStatus == QStringLiteral("no_workers")
                                       || jobStatus == QStringLiteral("remote_disabled")) {
                                statusCode = 503;
                                statusText = "Service Unavailable";
                            } else if (jobStatus == QStringLiteral("accepted")) {
                                statusCode = 202;
                                statusText = "Accepted";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/jobs/validate-output") {
                        QJsonParseError parseError;
                        const QJsonObject params = QJsonDocument::fromJson(requestBody, &parseError).object();
                        if (parseError.error != QJsonParseError::NoError || !onRequest_) {
                            statusCode = parseError.error != QJsonParseError::NoError ? 400 : 503;
                            statusText = statusCode == 400 ? "Bad Request" : "Service Unavailable";
                            body = {{QStringLiteral("error"), statusCode == 400
                                ? QStringLiteral("invalid_json") : QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("validateOutput"), params);
                            if (body.value(QStringLiteral("status")).toString() != QStringLiteral("ok")) {
                                statusCode = 422;
                                statusText = "Unprocessable Entity";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/alerts/clear") {
                        if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("clearLastAlert"), QJsonObject());
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/alerts/failure-threshold") {
                        QJsonParseError parseError;
                        const QJsonDocument document = QJsonDocument::fromJson(requestBody, &parseError);
                        const double fraction = document.object().value(QStringLiteral("fraction")).toDouble(-1.0);
                        if (parseError.error != QJsonParseError::NoError || fraction < 0.0 || fraction > 1.0) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_alert_threshold")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("setFailureAlertThreshold"),
                                              {{QStringLiteral("fraction"), fraction}});
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/templates/")
                               && requestLine[1].endsWith("/submit")) {
                        const QByteArray prefix = "/api/templates/";
                        const QByteArray suffix = "/submit";
                        const QString name = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        if (name.isEmpty() || !onRequest_) {
                            statusCode = name.isEmpty() ? 400 : 503;
                            statusText = name.isEmpty() ? "Bad Request" : "Service Unavailable";
                            body = {{QStringLiteral("error"), name.isEmpty()
                                ? QStringLiteral("invalid_template_request")
                                : QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("submitTemplate"),
                                              {{QStringLiteral("name"), name}});
                            const QString templateStatus = body.value(QStringLiteral("status")).toString();
                            if (templateStatus == QStringLiteral("template_not_found")) {
                                statusCode = 404;
                                statusText = "Not Found";
                            } else if (templateStatus == QStringLiteral("accepted")) {
                                statusCode = 202;
                                statusText = "Accepted";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/alerts/queue-depth") {
                        QJsonParseError parseError;
                        const QJsonObject params = QJsonDocument::fromJson(requestBody, &parseError).object();
                        const int count = params.value(QStringLiteral("count")).toInt(-1);
                        if (parseError.error != QJsonParseError::NoError || count < 0) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_queue_alert_threshold")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("setQueuedJobAlertThreshold"), params);
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/alerts/webhook") {
                        QJsonParseError parseError;
                        const QJsonObject params = QJsonDocument::fromJson(requestBody, &parseError).object();
                        const QString url = params.value(QStringLiteral("url")).toString().trimmed();
                        const QUrl parsedUrl(url);
                        if (parseError.error != QJsonParseError::NoError
                            || (!url.isEmpty() && (!parsedUrl.isValid()
                                || (parsedUrl.scheme() != QStringLiteral("http")
                                    && parsedUrl.scheme() != QStringLiteral("https"))))) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_webhook_url")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("setAlertWebhookUrl"), params);
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/queue/clear") {
                        if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("clearQueuedJobs"), QJsonObject());
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/jobs/resubmit") {
                        QJsonParseError parseError;
                        const QJsonObject params = QJsonDocument::fromJson(requestBody, &parseError).object();
                        if (parseError.error != QJsonParseError::NoError
                            || !params.value(QStringLiteral("jobIds")).isArray()) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_resubmit_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("resubmitJobs"), params);
                            statusCode = 202;
                            statusText = "Accepted";
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/queue/cancel") {
                        QJsonParseError parseError;
                        const QJsonObject params = QJsonDocument::fromJson(requestBody, &parseError).object();
                        if (parseError.error != QJsonParseError::NoError
                            || !params.value(QStringLiteral("jobIds")).isArray()) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_cancel_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("cancelJobs"), params);
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/queue/priority") {
                        QJsonParseError parseError;
                        const QJsonObject params = QJsonDocument::fromJson(requestBody, &parseError).object();
                        if (parseError.error != QJsonParseError::NoError
                            || !params.value(QStringLiteral("jobIds")).isArray()) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_priority_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("setJobPriorities"), params);
                            if (body.value(QStringLiteral("status")).toString()
                                == QStringLiteral("updated")) {
                                statusCode = 200;
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/jobs/")
                               && requestLine[1].endsWith("/priority")) {
                        const QByteArray prefix = "/api/jobs/";
                        const QByteArray suffix = "/priority";
                        const QString requestedJobId = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        QJsonParseError parseError;
                        const QJsonDocument document = QJsonDocument::fromJson(requestBody, &parseError);
                        const int priority = document.object().value(QStringLiteral("priority")).toInt();
                        if (parseError.error != QJsonParseError::NoError || requestedJobId.isEmpty()) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_priority_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("setJobPriority"),
                                              {{QStringLiteral("jobId"), requestedJobId},
                                               {QStringLiteral("priority"), priority}});
                            if (body.value(QStringLiteral("status")).toString()
                                == QStringLiteral("job_not_found")) {
                                statusCode = 404;
                                statusText = "Not Found";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/jobs/")
                               && requestLine[1].endsWith("/duplicate")) {
                        const QByteArray prefix = "/api/jobs/";
                        const QByteArray suffix = "/duplicate";
                        const QString requestedJobId = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        QJsonParseError parseError;
                        QJsonObject overrides;
                        if (requestBody.isEmpty()) {
                            parseError.error = QJsonParseError::NoError;
                        } else {
                            overrides = QJsonDocument::fromJson(requestBody, &parseError).object();
                        }
                        if (requestedJobId.isEmpty() || parseError.error != QJsonParseError::NoError) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_duplicate_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("duplicateJob"), overrides);
                            if (body.value(QStringLiteral("status")).toString()
                                == QStringLiteral("job_not_found_or_invalid")) {
                                statusCode = 404;
                                statusText = "Not Found";
                            } else if (body.value(QStringLiteral("status")).toString()
                                       == QStringLiteral("accepted")) {
                                statusCode = 202;
                                statusText = "Accepted";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/jobs/")
                               && requestLine[1].endsWith("/resubmit")) {
                        const QByteArray prefix = "/api/jobs/";
                        const QByteArray suffix = "/resubmit";
                        const QString requestedJobId = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        const bool knownJob = status.value(QStringLiteral("jobHistory")).toArray()
                            .contains(requestedJobId);
                        if (requestedJobId.isEmpty() || !knownJob) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("job_not_found")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("resubmitJob"),
                                              {{QStringLiteral("jobId"), requestedJobId}});
                            const QString resubmitStatus = body.value(QStringLiteral("status")).toString();
                            if (resubmitStatus == QStringLiteral("busy")) {
                                statusCode = 409;
                                statusText = "Conflict";
                            } else if (resubmitStatus == QStringLiteral("accepted")) {
                                statusCode = 202;
                                statusText = "Accepted";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/jobs/")
                               && (requestLine[1].endsWith("/cancel")
                                   || requestLine[1].endsWith("/pause")
                                   || requestLine[1].endsWith("/resume"))) {
                        const QByteArray prefix = "/api/jobs/";
                        const QByteArray suffix = requestLine[1].endsWith("/cancel")
                            ? QByteArray("/cancel")
                            : (requestLine[1].endsWith("/pause")
                                ? QByteArray("/pause") : QByteArray("/resume"));
                        const QString requestedJobId = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        const bool cancelRequest = suffix == QByteArray("/cancel");
                        const bool currentJob = status.value(QStringLiteral("jobId")).toString() == requestedJobId;
                        const bool queuedJob = status.value(QStringLiteral("queuedJobIds")).toArray()
                            .contains(requestedJobId);
                        if (requestedJobId.isEmpty() || (!currentJob && !(cancelRequest && queuedJob))) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("job_not_found")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            const QString method = suffix == QByteArray("/cancel")
                                ? QStringLiteral("cancelJob")
                                : (suffix == QByteArray("/pause")
                                    ? QStringLiteral("pauseJob") : QStringLiteral("resumeJob"));
                            body = onRequest_(method, cancelRequest
                                ? QJsonObject{{QStringLiteral("jobId"), requestedJobId}}
                                : QJsonObject());
                            const QString operationStatus = body.value(QStringLiteral("status")).toString();
                            if (operationStatus == QStringLiteral("cancel_requested")
                                || operationStatus == QStringLiteral("pause_requested")
                                || operationStatus == QStringLiteral("resume_requested")) {
                                statusCode = 202;
                                statusText = "Accepted";
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/rpc") {
                        QJsonParseError parseError;
                        const QJsonDocument document = QJsonDocument::fromJson(requestBody, &parseError);
                        if (parseError.error != QJsonParseError::NoError || document.isNull()
                            || (!document.isObject() && !document.isArray())
                            || (document.isArray() && document.array().isEmpty())) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_rpc_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else if (document.isArray()) {
                            responseIsArray = true;
                            for (const auto& value : document.array()) {
                                const QJsonObject requestObject = value.toObject();
                                const QString method = requestObject.value(QStringLiteral("method")).toString().trimmed();
                                QJsonObject result;
                                if (requestObject.isEmpty() || method.isEmpty()) {
                                    result = {{QStringLiteral("error"), QStringLiteral("invalid_rpc_request")}};
                                    statusCode = 400;
                                } else {
                                    result = onRequest_(method,
                                        requestObject.value(QStringLiteral("params")).toObject());
                                }
                                if (requestObject.contains(QStringLiteral("id")))
                                    result[QStringLiteral("id")] = requestObject.value(QStringLiteral("id"));
                                responseArray.append(result);
                            }
                        } else {
                            const QJsonObject requestObject = document.object();
                            const QString method = requestObject.value(QStringLiteral("method")).toString().trimmed();
                            if (method.isEmpty()) {
                                statusCode = 400;
                                statusText = "Bad Request";
                                body = {{QStringLiteral("error"), QStringLiteral("invalid_rpc_request")}};
                            } else {
                            body = onRequest_(method,
                                requestObject.value(QStringLiteral("params")).toObject());
                            const QString rpcStatus = body.value(QStringLiteral("status")).toString();
                            if (rpcStatus == QStringLiteral("invalid_request")
                                || rpcStatus == QStringLiteral("renderer_not_found")
                                || rpcStatus == QStringLiteral("submit_failed")) {
                                statusCode = 400;
                                statusText = "Bad Request";
                            } else if (rpcStatus == QStringLiteral("job_not_found")) {
                                statusCode = 404;
                                statusText = "Not Found";
                            } else if (rpcStatus == QStringLiteral("template_not_found")) {
                                statusCode = 404;
                                statusText = "Not Found";
                            } else if (rpcStatus == QStringLiteral("busy")) {
                                statusCode = 409;
                                statusText = "Conflict";
                            } else if (rpcStatus == QStringLiteral("no_workers")
                                       || rpcStatus == QStringLiteral("remote_disabled")) {
                                statusCode = 503;
                                statusText = "Service Unavailable";
                            } else if (rpcStatus == QStringLiteral("accepted")
                                       || rpcStatus == QStringLiteral("cancel_requested")
                                       || rpcStatus == QStringLiteral("pause_requested")
                                       || rpcStatus == QStringLiteral("resume_requested")) {
                                statusCode = 202;
                                statusText = "Accepted";
                            }
                            }
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/workers/")
                               && requestLine[1].endsWith("/maintenance")) {
                        const QByteArray prefix = "/api/workers/";
                        const QByteArray suffix = "/maintenance";
                        const QByteArray workerId = requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size());
                        QJsonParseError parseError;
                        const QJsonDocument document = QJsonDocument::fromJson(requestBody, &parseError);
                        const bool maintenance = document.isObject()
                            && document.object().value(QStringLiteral("maintenance")).toBool(false);
                        bool updated = false;
                        if (parseError.error == QJsonParseError::NoError && !workerId.isEmpty()) {
                            std::lock_guard<std::mutex> lock(mutex_);
                            const auto socketIt = workerSockets_.find(QString::fromUtf8(workerId));
                            if (socketIt != workerSockets_.end()) {
                                const auto workerIt = workers_.find(socketIt->second);
                                if (workerIt != workers_.end()) {
                                    workerIt->second.capabilities[QStringLiteral("maintenance")] = maintenance;
                                    if (workerIt->second.assignedFrames == 0) {
                                        workerIt->second.state = maintenance
                                            ? QStringLiteral("Maintenance") : QStringLiteral("Idle");
                                    }
                                    updated = true;
                                }
                            }
                        }
                        if (!updated) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("worker_not_found")}};
                        } else {
                            body = {{QStringLiteral("workerId"), QString::fromUtf8(workerId)},
                                    {QStringLiteral("maintenance"), maintenance}};
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "DELETE"
                               && requestLine[1].startsWith("/api/templates/")) {
                        const QString name = QString::fromUtf8(
                            requestLine[1].mid(QByteArray("/api/templates/").size()));
                        if (name.isEmpty() || !onRequest_) {
                            statusCode = name.isEmpty() ? 400 : 503;
                            statusText = name.isEmpty() ? "Bad Request" : "Service Unavailable";
                            body = {{QStringLiteral("error"), name.isEmpty()
                                ? QStringLiteral("invalid_template_request")
                                : QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("removeTemplate"),
                                              {{QStringLiteral("name"), name}});
                            if (body.value(QStringLiteral("status")).toString()
                                == QStringLiteral("template_not_found")) {
                                statusCode = 404;
                                statusText = "Not Found";
                            }
                        }
                    } else if (requestLine.size() < 2 || requestLine[0] != "GET") {
                        statusCode = 405;
                        statusText = "Method Not Allowed";
                        body = {{QStringLiteral("error"), QStringLiteral("method_not_allowed")}};
                    } else if (requestLine[1] == "/" || requestLine[1] == "/dashboard") {
                        contentType = "text/html; charset=utf-8";
                        metricsPayload = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Artifact Render Farm</title>
<style>body{font:14px system-ui;margin:24px;background:#15171b;color:#e8eaed}main{max-width:1100px;margin:auto}
section{background:#20242b;border:1px solid #353b45;border-radius:8px;padding:16px;margin:12px 0}
h1{font-size:22px}pre{white-space:pre-wrap;color:#b9d7ff}button{margin-right:8px;padding:6px 12px}
.ok{color:#8fe388}.bad{color:#ff9b9b}</style></head>
<body><main><h1>Artifact Render Farm</h1><section><div id="status">Loading…</div>
<p><button onclick="operate('pause')">Pause</button><button onclick="operate('resume')">Resume</button>
<button onclick="operate('cancel')">Cancel</button><button onclick="clearQueue()">Clear queue</button>
<button onclick="clearAlert()">Clear alert</button></p></section>
<section><h2>Queued jobs</h2><pre id="queue">Loading…</pre></section>
<section><h2>Workers</h2><pre id="workers">Loading…</pre></section>
<section><h2>Job history</h2><pre id="history">Loading…</pre></section>
<section><h2>Worker logs</h2><pre id="logs">Loading…</pre></section></main>
<script>async function refresh(){try{const [s,w,l]=await Promise.all([fetch('/api/status'),fetch('/api/workers'),fetch('/api/logs')]);
const status=await s.json(),workers=await w.json(),logs=await l.json();document.getElementById('status').innerHTML=
'<b>Status:</b> '+status.status+' &nbsp; <b>Frames:</b> '+status.completedFrames+'/'+status.totalFrames+
' &nbsp; <b>ETA:</b> '+(status.estimatedRemainingMs??'—')+' ms'+
' &nbsp; <b>Est. total:</b> '+(status.estimatedCostMs??'—')+' ms'+
' &nbsp; <b>Queued:</b> '+(status.queuedJobs??0)+
' &nbsp; <b>Alert:</b> '+(status.failureAlertThreshold??0)+
' &nbsp; <b>Last alert:</b> '+(status.lastAlertType||'—')+' '+(status.lastAlertAt||'')+
' ('+(status.lastAlertFailedFrames??0)+' failed)';
document.getElementById('workers').textContent=JSON.stringify(workers,null,2);
document.getElementById('queue').textContent=JSON.stringify(status.queuedJobIds||[],null,2)+'\n'+
JSON.stringify(status.queuedPriorities||[],null,2);
document.getElementById('history').textContent=JSON.stringify(status.jobHistoryDetails||status.jobHistory||[],null,2);
document.getElementById('logs').textContent=JSON.stringify(logs.logs||[],null,2)}catch(e){
document.getElementById('status').textContent='Dashboard unavailable: '+e}}refresh();setInterval(refresh,2000)</script>
<script>async function operate(action){try{const s=await fetch('/api/status');const j=await s.json();if(!j.jobId)return;
await fetch('/api/jobs/'+encodeURIComponent(j.jobId)+'/'+action,{method:'POST'});refresh()}catch(e){alert(e)}}</script>
<script>async function clearQueue(){if(!confirm('Clear all queued jobs?'))return;
await fetch('/api/queue/clear',{method:'POST'});refresh()}</script>
<script>async function clearAlert(){await fetch('/api/alerts/clear',{method:'POST'});refresh()}</script>
</body></html>)HTML";
                    } else if (requestLine[1] == "/api/alerts") {
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        body = QJsonObject{
                            {QStringLiteral("failureAlertThreshold"),
                             status.value(QStringLiteral("failureAlertThreshold")).toDouble()},
                            {QStringLiteral("lastAlertType"), status.value(QStringLiteral("lastAlertType")).toString()},
                            {QStringLiteral("lastAlertAt"), status.value(QStringLiteral("lastAlertAt")).toString()},
                            {QStringLiteral("lastAlertFailedFrames"),
                             status.value(QStringLiteral("lastAlertFailedFrames")).toInt()},
                            {QStringLiteral("queuedJobAlertThreshold"),
                             status.value(QStringLiteral("queuedJobAlertThreshold")).toInt()},
                            {QStringLiteral("lastAlertQueuedJobs"),
                             status.value(QStringLiteral("lastAlertQueuedJobs")).toInt()}
                        };
                    } else if (requestLine[1] == "/api/templates") {
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        body = QJsonObject{
                            {QStringLiteral("templates"), status.value(QStringLiteral("templates")).toArray()}
                        };
                    } else if (requestLine[1].startsWith("/api/templates/")) {
                        const QString name = QString::fromUtf8(
                            requestLine[1].mid(QByteArray("/api/templates/").size()));
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        const QJsonArray details = status.value(QStringLiteral("templateDetails")).toArray();
                        for (const auto& value : details) {
                            if (value.toObject().value(QStringLiteral("name")).toString() == name) {
                                body = value.toObject();
                                break;
                            }
                        }
                        if (body.isEmpty()) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("template_not_found")}};
                        }
                    } else if (requestLine[1] == "/api/queue") {
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        body = QJsonObject{
                            {QStringLiteral("queuedJobs"), status.value(QStringLiteral("queuedJobs")).toInt()},
                            {QStringLiteral("queuedJobIds"), status.value(QStringLiteral("queuedJobIds")).toArray()},
                            {QStringLiteral("queuedPriorities"), status.value(QStringLiteral("queuedPriorities")).toArray()}
                        };
                    } else if (requestLine[1] == "/api/status"
                               || requestLine[1] == "/api/jobs"
                               || requestLine[1] == "/api/history") {
                        if (httpStatusProvider_) {
                            body = httpStatusProvider_();
                        } else {
                            int workerCount = 0;
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                workerCount = static_cast<int>(workers_.size());
                            }
                            body = {{QStringLiteral("status"), QStringLiteral("ok")},
                                    {QStringLiteral("workers"), workerCount}};
                        }
                        if (requestLine[1] == "/api/history") {
                            body = QJsonObject{
                                {QStringLiteral("jobHistory"),
                                 body.value(QStringLiteral("jobHistory")).toArray()},
                                {QStringLiteral("jobHistoryDetails"),
                                 body.value(QStringLiteral("jobHistoryDetails")).toArray()}
                            };
                        }
                    } else if (requestLine[1] == "/api/logs") {
                        std::lock_guard<std::mutex> lock(mutex_);
                        QJsonArray logs;
                        const int logCount = static_cast<int>(workerLogs_.size());
                        const int first = std::max(0, logCount - 200);
                        for (int i = first; i < logCount; ++i)
                            logs.append(workerLogs_.at(i));
                        body = QJsonObject{{QStringLiteral("logs"), logs}};
                    } else if (requestLine[1].startsWith("/api/jobs/")) {
                        const QString requestedJobId = QString::fromUtf8(
                            requestLine[1].mid(QByteArray("/api/jobs/").size()));
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        if (status.value(QStringLiteral("jobId")).toString() == requestedJobId) {
                            body = status;
                        } else {
                            const QJsonArray details = status.value(QStringLiteral("jobHistoryDetails")).toArray();
                            for (const auto& value : details) {
                                if (value.toObject().value(QStringLiteral("jobId")).toString()
                                    == requestedJobId) {
                                    body = value.toObject();
                                    break;
                                }
                            }
                        }
                        if (body.isEmpty()) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("job_not_found")}};
                        }
                    } else if (requestLine[1] == "/api/health") {
                        int workerCount = 0;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            workerCount = static_cast<int>(workers_.size());
                            for (const auto& [_, worker] : workers_)
                                totalWorkerRenderTimeMs += worker.totalRenderTimeMs;
                        }
                        body = {{QStringLiteral("status"), QStringLiteral("ok")},
                                {QStringLiteral("workers"), workerCount}};
                    } else if (requestLine[1] == "/api/capabilities") {
                        body = QJsonObject{
                            {QStringLiteral("tls"), tlsEnabled_},
                            {QStringLiteral("endpoints"), QJsonArray{
                                QStringLiteral("GET /api/health"),
                                QStringLiteral("GET / or /dashboard"),
                                QStringLiteral("GET /api/status"),
                                QStringLiteral("GET /api/jobs"),
                                QStringLiteral("GET /api/history"),
                                QStringLiteral("GET /api/queue"),
                                QStringLiteral("GET /api/templates"),
                                QStringLiteral("GET /api/alerts"),
                                QStringLiteral("GET /api/templates/{name}"),
                                QStringLiteral("POST /api/templates/{name}/submit"),
                                QStringLiteral("DELETE /api/templates/{name}"),
                                QStringLiteral("POST /api/alerts/failure-threshold"),
                                QStringLiteral("POST /api/alerts/queue-depth"),
                                QStringLiteral("POST /api/alerts/webhook"),
                                QStringLiteral("POST /api/alerts/clear"),
                                QStringLiteral("GET /api/logs"),
                                QStringLiteral("GET /api/jobs/{jobId}"),
                                QStringLiteral("POST /api/jobs"),
                                QStringLiteral("POST /api/jobs/validate-output"),
                                QStringLiteral("POST /api/jobs/{jobId}/cancel|pause|resume|resubmit"),
                                QStringLiteral("POST /api/jobs/{jobId}/duplicate"),
                                QStringLiteral("POST /api/jobs/resubmit"),
                                QStringLiteral("POST /api/jobs/{jobId}/priority"),
                                QStringLiteral("POST /api/queue/priority"),
                                QStringLiteral("POST /api/queue/cancel"),
                                QStringLiteral("POST /api/queue/clear"),
                                QStringLiteral("GET /api/workers"),
                                QStringLiteral("GET /api/workers/{workerId}"),
                                QStringLiteral("GET /api/workers/{workerId}/health"),
                                QStringLiteral("GET /api/workers/{workerId}/logs"),
                                QStringLiteral("POST /api/workers/{workerId}/maintenance"),
                                QStringLiteral("POST /api/rpc[batch] (submitJob[compositionId, workerPool, chunks], validateOutput, cancelJob, clearQueuedJobs, setJobPriority, setJobPriorities, duplicateJob, setFailureAlertThreshold)"),
                                QStringLiteral("GET /metrics")
                            }}
                        };
                    } else if (requestLine[1] == "/api/workers") {
                        QJsonArray workersJson;
                        std::lock_guard<std::mutex> lock(mutex_);
                        for (const auto& [_, worker] : workers_) {
                            const qint64 heartbeatAgeMs = worker.lastHeartbeat > 0
                                ? std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - worker.lastHeartbeat)
                                : -1;
                            workersJson.append(QJsonObject{
                                {QStringLiteral("workerId"), worker.workerId},
                                {QStringLiteral("address"), worker.address},
                                {QStringLiteral("state"), worker.state},
                                {QStringLiteral("assignedFrames"), worker.assignedFrames},
                                {QStringLiteral("completedFrames"), worker.completedFrames},
                                {QStringLiteral("failedFrames"), worker.failedFrames},
                                {QStringLiteral("renderTimeMs"), worker.renderTimeMs},
                                {QStringLiteral("totalRenderTimeMs"), worker.totalRenderTimeMs},
                                {QStringLiteral("currentFrame"), worker.currentFrame},
                                {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat},
                                {QStringLiteral("heartbeatAgeMs"), heartbeatAgeMs},
                                {QStringLiteral("healthy"), worker.connected && heartbeatAgeMs >= 0
                                    && heartbeatAgeMs <= HEARTBEAT_TIMEOUT_MS},
                                {QStringLiteral("capabilities"), worker.capabilities}
                            });
                        }
                        body = {{QStringLiteral("workers"), workersJson}};
                    } else if (requestLine[1].startsWith("/api/workers/")
                               && requestLine[1].endsWith("/logs")) {
                        const QByteArray prefix = "/api/workers/";
                        const QByteArray suffix = "/logs";
                        const QString requestedWorkerId = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        QJsonArray logs;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            for (const auto& value : workerLogs_) {
                                if (value.toObject().value(QStringLiteral("workerId")).toString()
                                    == requestedWorkerId)
                                    logs.append(value);
                            }
                        }
                        const int logCount = static_cast<int>(logs.size());
                        const int first = std::max(0, logCount - 200);
                        QJsonArray recentLogs;
                        for (int i = first; i < logCount; ++i)
                            recentLogs.append(logs.at(i));
                        body = QJsonObject{{QStringLiteral("workerId"), requestedWorkerId},
                                           {QStringLiteral("logs"), recentLogs}};
                    } else if (requestLine[1].startsWith("/api/workers/")) {
                        const QByteArray workerPrefix = "/api/workers/";
                        const QByteArray healthSuffix = "/health";
                        const bool healthRequest = requestLine[1].endsWith(healthSuffix);
                        const QByteArray workerId = healthRequest
                            ? requestLine[1].mid(workerPrefix.size(),
                                requestLine[1].size() - workerPrefix.size() - healthSuffix.size())
                            : requestLine[1].mid(workerPrefix.size());
                        std::lock_guard<std::mutex> lock(mutex_);
                        const auto socketIt = workerSockets_.find(QString::fromUtf8(workerId));
                        const auto workerIt = socketIt == workerSockets_.end()
                            ? workers_.end() : workers_.find(socketIt->second);
                        if (workerIt == workers_.end()) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("worker_not_found")}};
                        } else {
                            const auto& worker = workerIt->second;
                            const qint64 heartbeatAgeMs = worker.lastHeartbeat > 0
                                ? std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - worker.lastHeartbeat)
                                : -1;
                            const bool healthy = worker.connected && heartbeatAgeMs >= 0
                                && heartbeatAgeMs <= HEARTBEAT_TIMEOUT_MS;
                            if (healthRequest && !healthy) {
                                statusCode = 503;
                                statusText = "Service Unavailable";
                            }
                            body = healthRequest ? QJsonObject{
                                {QStringLiteral("status"), healthy ? QStringLiteral("ok") : QStringLiteral("unhealthy")},
                                {QStringLiteral("workerId"), worker.workerId},
                                {QStringLiteral("state"), worker.state},
                                {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat},
                                {QStringLiteral("heartbeatAgeMs"), heartbeatAgeMs},
                                {QStringLiteral("healthy"), healthy}
                            } : QJsonObject{
                                {QStringLiteral("workerId"), worker.workerId},
                                {QStringLiteral("address"), worker.address},
                                {QStringLiteral("state"), worker.state},
                                {QStringLiteral("assignedFrames"), worker.assignedFrames},
                                {QStringLiteral("completedFrames"), worker.completedFrames},
                                {QStringLiteral("failedFrames"), worker.failedFrames},
                            {QStringLiteral("renderTimeMs"), worker.renderTimeMs},
                            {QStringLiteral("totalRenderTimeMs"), worker.totalRenderTimeMs},
                                {QStringLiteral("currentFrame"), worker.currentFrame},
                                {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat},
                                {QStringLiteral("heartbeatAgeMs"), heartbeatAgeMs},
                                {QStringLiteral("healthy"), healthy},
                                {QStringLiteral("capabilities"), worker.capabilities}
                            };
                        }
                    } else if (requestLine[1] == "/metrics") {
                        QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        int workerCount = 0;
                        int healthyWorkerCount = 0;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            workerCount = static_cast<int>(workers_.size());
                            const qint64 now = QDateTime::currentMSecsSinceEpoch();
                            for (const auto& [_, worker] : workers_) {
                                totalWorkerRenderTimeMs += worker.totalRenderTimeMs;
                                const qint64 heartbeatAgeMs = worker.lastHeartbeat > 0
                                    ? std::max<qint64>(0, now - worker.lastHeartbeat) : -1;
                                if (worker.connected && heartbeatAgeMs >= 0
                                    && heartbeatAgeMs <= HEARTBEAT_TIMEOUT_MS)
                                    ++healthyWorkerCount;
                            }
                        }
                        const auto metric = [&status](const QString& key, const QString& name) {
                            return name.toUtf8() + " " + QByteArray::number(status.value(key).toDouble());
                        };
                        metricsPayload = "# TYPE artifact_farm_workers gauge\n"
                            "artifact_farm_workers " + QByteArray::number(workerCount) + "\n"
                            "# TYPE artifact_farm_healthy_workers gauge\n"
                            "artifact_farm_healthy_workers " + QByteArray::number(healthyWorkerCount) + "\n"
                            "# TYPE artifact_farm_queued_jobs gauge\n"
                            + metric(QStringLiteral("queuedJobs"), QStringLiteral("artifact_farm_queued_jobs")) + "\n"
                            "# TYPE artifact_farm_worker_render_time_ms counter\n"
                            "artifact_farm_worker_render_time_ms " + QByteArray::number(totalWorkerRenderTimeMs) + "\n"
                            "# TYPE artifact_farm_busy gauge\n"
                            "artifact_farm_busy " + QByteArray::number(status.value(QStringLiteral("busy")).toBool() ? 1 : 0) + "\n"
                            "# TYPE artifact_farm_preemptions counter\n"
                            "artifact_farm_preemptions " + QByteArray::number(status.value(QStringLiteral("preemptionCount")).toInt()) + "\n"
                            "# TYPE artifact_farm_success gauge\n"
                            "artifact_farm_success " + QByteArray::number(status.value(QStringLiteral("success")).toBool() ? 1 : 0) + "\n"
                            "# TYPE artifact_farm_frames_completed gauge\n"
                            + metric(QStringLiteral("completedFrames"), QStringLiteral("artifact_farm_frames_completed")) + "\n"
                            "# TYPE artifact_farm_frames_failed gauge\n"
                            + metric(QStringLiteral("failedFrames"), QStringLiteral("artifact_farm_frames_failed")) + "\n"
                            "# TYPE artifact_farm_frames_total gauge\n"
                            + metric(QStringLiteral("totalFrames"), QStringLiteral("artifact_farm_frames_total")) + "\n"
                            "# TYPE artifact_farm_elapsed_ms gauge\n"
                            + metric(QStringLiteral("elapsedMs"), QStringLiteral("artifact_farm_elapsed_ms")) + "\n"
                            "# TYPE artifact_farm_estimated_remaining_ms gauge\n"
                            + metric(QStringLiteral("estimatedRemainingMs"), QStringLiteral("artifact_farm_estimated_remaining_ms")) + "\n";
                        metricsPayload += "# TYPE artifact_farm_estimated_cost_ms gauge\n"
                            + metric(QStringLiteral("estimatedCostMs"), QStringLiteral("artifact_farm_estimated_cost_ms")) + "\n";
                        contentType = "text/plain; version=0.0.4";
                    } else {
                        statusCode = 404;
                        statusText = "Not Found";
                        body = {{QStringLiteral("error"), QStringLiteral("not_found")}};
                    }
                    const QByteArray payload = contentType.startsWith("text/")
                        ? metricsPayload : (responseIsArray
                            ? QJsonDocument(responseArray).toJson(QJsonDocument::Compact)
                            : QJsonDocument(body).toJson(QJsonDocument::Compact));
                    const QByteArray response = "HTTP/1.1 " + QByteArray::number(statusCode)
                        + " " + statusText + "\r\nContent-Type: " + contentType + "\r\n"
                        + "Cache-Control: no-store\r\n"
                        + "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                        + "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
                        + "Content-Length: " + QByteArray::number(payload.size())
                        + "\r\nConnection: close\r\n\r\n" + payload;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    QTcpSocket* findSocket(const QString& workerId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = workerSockets_.find(workerId);
        return (it != workerSockets_.end()) ? it->second : nullptr;
    }
};

// -- NetworkPCServer public API --

NetworkPCServer::NetworkPCServer()
    : impl_(new Impl())
{}

NetworkPCServer::~NetworkPCServer() {
    delete impl_;
}

bool NetworkPCServer::start(unsigned short port) {
    if (impl_->running_) return true;
    if (!impl_->server_->listen(QHostAddress::Any, port)) return false;
    impl_->port_ = port;
    impl_->running_ = true;
    impl_->setupConnections();
    impl_->scheduleHeartbeatCheck();
    qDebug() << "[Farm] RPC server on port" << port;
    return true;
}

void NetworkPCServer::stop() { impl_->stopInternal(); }
bool NetworkPCServer::isRunning() const { return impl_->running_; }
unsigned short NetworkPCServer::port() const { return impl_->port_; }

bool NetworkPCServer::startHttpApi(unsigned short port) {
    if (impl_->httpServer_->isListening()) return true;
    if (port == 0) port = 9877;
    if (!impl_->httpServer_->listen(QHostAddress::Any, port)) return false;
    impl_->httpPort_ = impl_->httpServer_->serverPort();
    impl_->setupHttpApi();
    qDebug() << "[Farm] HTTP API on port" << impl_->httpPort_;
    return true;
}

void NetworkPCServer::stopHttpApi() { impl_->stopHttpApi(); }
bool NetworkPCServer::isHttpApiRunning() const { return impl_->httpServer_->isListening(); }
unsigned short NetworkPCServer::httpApiPort() const { return impl_->httpPort_; }

QString NetworkPCServer::call(const QString& function, const QStringList& args) {
    QJsonObject params;
    QJsonArray arr;
    for (const auto& a : args) arr.append(a);
    params["args"] = arr;

    QJsonObject result;
    if (impl_->onRequest_)
        result = impl_->onRequest_(function, params);
    return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

QString NetworkPCServer::callWorker(const QString& wid, const QString& method, const QJsonObject& params) {
    QTcpSocket* s = impl_->findSocket(wid);
    if (!s) return "{}";
    QJsonObject msg;
    msg["jsonrpc"] = "2.0"; msg["method"] = method; msg["params"] = params;
    msg["id"] = static_cast<qint64>(impl_->nextRpcId_++);
    impl_->sendJson(s, msg);
    return "{}";
}

bool NetworkPCServer::sendJobAssignment(const QString& wid, const QJsonObject& jobJson) {
    QTcpSocket* s = impl_->findSocket(wid);
    if (!s) return false;
    QJsonObject msg;
    msg["jsonrpc"] = "2.0"; msg["method"] = "assignJob"; msg["params"] = jobJson;
    msg["id"] = static_cast<qint64>(impl_->nextRpcId_++);
    impl_->sendJson(s, msg);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        auto it = impl_->workerSockets_.find(wid);
        if (it != impl_->workerSockets_.end()) {
            auto wit = impl_->workers_.find(it->second);
            if (wit != impl_->workers_.end())
            {
                const int start = jobJson[QStringLiteral("startFrame")].toInt(0);
                const int end = jobJson[QStringLiteral("endFrame")].toInt(0);
                const int step = std::max(1, jobJson[QStringLiteral("step")].toInt(1));
                const int frameCount = end > start ? (end - start + step - 1) / step : 0;
                wit->second.assignedFrames += frameCount;
                if (frameCount > 0)
                    wit->second.state = QStringLiteral("Rendering");
            }
        }
    }
    return true;
}

QJsonObject NetworkPCServer::requestWorkerStatus(const QString& wid) {
    QTcpSocket* s = impl_->findSocket(wid);
    if (!s) return {{"error", "not found"}};
    QJsonObject msg;
    msg["jsonrpc"] = "2.0"; msg["method"] = "status"; msg["params"] = QJsonObject(); msg["id"] = 1;
    impl_->sendJson(s, msg);
    return {{"status", "alive"}, {"workerId", wid}};
}

void NetworkPCServer::setOnWorkerConnected(WorkerConnectedCallback cb) { impl_->onWorkerConnected_ = std::move(cb); }
void NetworkPCServer::setOnWorkerDisconnected(WorkerDisconnectedCallback cb) { impl_->onWorkerDisconnected_ = std::move(cb); }
void NetworkPCServer::setOnWorkerHeartbeat(WorkerHeartbeatCallback cb) { impl_->onWorkerHeartbeat_ = std::move(cb); }
void NetworkPCServer::setOnRequest(RpcRequestHandler handler) { impl_->onRequest_ = std::move(handler); }
void NetworkPCServer::setHttpStatusProvider(HttpStatusProvider provider) {
    impl_->httpStatusProvider_ = std::move(provider);
}
void NetworkPCServer::setAuthToken(const QString& token) { impl_->authToken_ = token; }

bool NetworkPCServer::setWorkerMaintenance(const QString& workerId, bool maintenance) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    const auto socketIt = impl_->workerSockets_.find(workerId);
    if (socketIt == impl_->workerSockets_.end()) return false;
    const auto workerIt = impl_->workers_.find(socketIt->second);
    if (workerIt == impl_->workers_.end()) return false;
    workerIt->second.capabilities[QStringLiteral("maintenance")] = maintenance;
    if (workerIt->second.assignedFrames == 0) {
        workerIt->second.state = maintenance ? QStringLiteral("Maintenance")
                                             : QStringLiteral("Idle");
    }
    return true;
}

bool NetworkPCServer::setTlsCertificateFiles(const QString& certificateFile,
                                             const QString& privateKeyFile) {
    QFile certificateInput(certificateFile);
    QFile privateKeyInput(privateKeyFile);
    if (!certificateInput.open(QIODevice::ReadOnly) ||
        !privateKeyInput.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto certificates = QSslCertificate::fromData(
        certificateInput.readAll(), QSsl::Pem);
    const QSslKey key(privateKeyInput.readAll(), QSsl::Rsa,
                      QSsl::Pem, QSsl::PrivateKey);
    if (certificates.isEmpty() || key.isNull()) return false;
    impl_->tlsCertificate_ = certificates.front();
    impl_->tlsPrivateKey_ = key;
    impl_->tlsEnabled_ = true;
    impl_->server_->certificate = impl_->tlsCertificate_;
    impl_->server_->privateKey = impl_->tlsPrivateKey_;
    impl_->server_->tlsEnabled = true;
    impl_->httpServer_->certificate = impl_->tlsCertificate_;
    impl_->httpServer_->privateKey = impl_->tlsPrivateKey_;
    impl_->httpServer_->tlsEnabled = true;
    return true;
}

bool NetworkPCServer::tlsEnabled() const { return impl_->tlsEnabled_; }

std::vector<RemoteWorkerInfo> NetworkPCServer::connectedWorkers() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    std::vector<RemoteWorkerInfo> r;
    for (const auto& [_, info] : impl_->workers_) r.push_back(info);
    return r;
}

RemoteWorkerInfo NetworkPCServer::workerInfo(const QString& wid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->workerSockets_.find(wid);
    if (it == impl_->workerSockets_.end()) return {};
    auto wit = impl_->workers_.find(it->second);
    if (wit == impl_->workers_.end()) return {};
    return wit->second;
}

bool NetworkPCServer::hasWorker(const QString& wid) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->workerSockets_.find(wid) != impl_->workerSockets_.end();
}

int NetworkPCServer::activeWorkerCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return static_cast<int>(impl_->workers_.size());
}

NetworkPCServer& NetworkPCServer::instance() {
    static NetworkPCServer s_instance;
    return s_instance;
}

}
