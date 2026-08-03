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
            || method == QStringLiteral("frameFailed");
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
                    workerIt->second.renderTimeMs = std::max<qint64>(
                        0, params[QStringLiteral("renderTimeMs")].toVariant().toLongLong());
                    workerIt->second.currentFrame = params[QStringLiteral("currentFrame")].toInt(-1);
                }
            }
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
                    QByteArray contentType = "application/json";
                    QByteArray metricsPayload;
                    int statusCode = 200;
                    QByteArray statusText = "OK";
                    if (!authorized) {
                        statusCode = 401;
                        statusText = "Unauthorized";
                        body = {{QStringLiteral("error"), QStringLiteral("unauthorized")}};
                    } else if (requestLine.size() >= 2 && requestLine[0] == "OPTIONS") {
                        statusCode = 204;
                        statusText = "No Content";
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1].startsWith("/api/jobs/")
                               && requestLine[1].endsWith("/cancel")) {
                        const QByteArray prefix = "/api/jobs/";
                        const QByteArray suffix = "/cancel";
                        const QString requestedJobId = QString::fromUtf8(requestLine[1].mid(
                            prefix.size(), requestLine[1].size() - prefix.size() - suffix.size()));
                        const QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        if (requestedJobId.isEmpty()
                            || status.value(QStringLiteral("jobId")).toString() != requestedJobId) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("job_not_found")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(QStringLiteral("cancelJob"), QJsonObject());
                        }
                    } else if (requestLine.size() >= 2 && requestLine[0] == "POST"
                               && requestLine[1] == "/api/rpc") {
                        QJsonParseError parseError;
                        const QJsonDocument document = QJsonDocument::fromJson(requestBody, &parseError);
                        const QJsonObject requestObject = document.object();
                        const QString method = requestObject.value(QStringLiteral("method")).toString().trimmed();
                        if (parseError.error != QJsonParseError::NoError || method.isEmpty()) {
                            statusCode = 400;
                            statusText = "Bad Request";
                            body = {{QStringLiteral("error"), QStringLiteral("invalid_rpc_request")}};
                        } else if (!onRequest_) {
                            statusCode = 503;
                            statusText = "Service Unavailable";
                            body = {{QStringLiteral("error"), QStringLiteral("rpc_handler_unavailable")}};
                        } else {
                            body = onRequest_(method,
                                requestObject.value(QStringLiteral("params")).toObject());
                            const QString rpcStatus = body.value(QStringLiteral("status")).toString();
                            if (rpcStatus == QStringLiteral("invalid_request")
                                || rpcStatus == QStringLiteral("renderer_not_found")) {
                                statusCode = 400;
                                statusText = "Bad Request";
                            } else if (rpcStatus == QStringLiteral("busy")) {
                                statusCode = 409;
                                statusText = "Conflict";
                            } else if (rpcStatus == QStringLiteral("no_workers")
                                       || rpcStatus == QStringLiteral("remote_disabled")) {
                                statusCode = 503;
                                statusText = "Service Unavailable";
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
                    } else if (requestLine.size() < 2 || requestLine[0] != "GET") {
                        statusCode = 405;
                        statusText = "Method Not Allowed";
                        body = {{QStringLiteral("error"), QStringLiteral("method_not_allowed")}};
                    } else if (requestLine[1] == "/api/status" || requestLine[1] == "/api/jobs") {
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
                    } else if (requestLine[1].startsWith("/api/jobs/")) {
                        const QString requestedJobId = QString::fromUtf8(
                            requestLine[1].mid(QByteArray("/api/jobs/").size()));
                        body = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        if (body.value(QStringLiteral("jobId")).toString() != requestedJobId) {
                            statusCode = 404;
                            statusText = "Not Found";
                            body = {{QStringLiteral("error"), QStringLiteral("job_not_found")}};
                        }
                    } else if (requestLine[1] == "/api/health") {
                        int workerCount = 0;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            workerCount = static_cast<int>(workers_.size());
                        }
                        body = {{QStringLiteral("status"), QStringLiteral("ok")},
                                {QStringLiteral("workers"), workerCount}};
                    } else if (requestLine[1] == "/api/capabilities") {
                        body = QJsonObject{
                            {QStringLiteral("tls"), tlsEnabled_},
                            {QStringLiteral("endpoints"), QJsonArray{
                                QStringLiteral("GET /api/health"),
                                QStringLiteral("GET /api/status"),
                                QStringLiteral("GET /api/jobs"),
                                QStringLiteral("GET /api/jobs/{jobId}"),
                                QStringLiteral("POST /api/jobs/{jobId}/cancel"),
                                QStringLiteral("GET /api/workers"),
                                QStringLiteral("GET /api/workers/{workerId}"),
                                QStringLiteral("GET /api/workers/{workerId}/health"),
                                QStringLiteral("POST /api/workers/{workerId}/maintenance"),
                                QStringLiteral("POST /api/rpc (submitJob[compositionId, workerPool], cancelJob)"),
                                QStringLiteral("GET /metrics")
                            }}
                        };
                    } else if (requestLine[1] == "/api/workers") {
                        QJsonArray workersJson;
                        std::lock_guard<std::mutex> lock(mutex_);
                        for (const auto& [_, worker] : workers_) {
                            workersJson.append(QJsonObject{
                                {QStringLiteral("workerId"), worker.workerId},
                                {QStringLiteral("address"), worker.address},
                                {QStringLiteral("state"), worker.state},
                                {QStringLiteral("assignedFrames"), worker.assignedFrames},
                                {QStringLiteral("completedFrames"), worker.completedFrames},
                                {QStringLiteral("failedFrames"), worker.failedFrames},
                                {QStringLiteral("renderTimeMs"), worker.renderTimeMs},
                                {QStringLiteral("currentFrame"), worker.currentFrame},
                                {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat},
                                {QStringLiteral("capabilities"), worker.capabilities}
                            });
                        }
                        body = {{QStringLiteral("workers"), workersJson}};
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
                            body = healthRequest ? QJsonObject{
                                {QStringLiteral("status"), QStringLiteral("ok")},
                                {QStringLiteral("workerId"), worker.workerId},
                                {QStringLiteral("state"), worker.state},
                                {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat}
                            } : QJsonObject{
                                {QStringLiteral("workerId"), worker.workerId},
                                {QStringLiteral("address"), worker.address},
                                {QStringLiteral("state"), worker.state},
                                {QStringLiteral("assignedFrames"), worker.assignedFrames},
                                {QStringLiteral("completedFrames"), worker.completedFrames},
                                {QStringLiteral("failedFrames"), worker.failedFrames},
                                {QStringLiteral("renderTimeMs"), worker.renderTimeMs},
                                {QStringLiteral("currentFrame"), worker.currentFrame},
                                {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat},
                                {QStringLiteral("capabilities"), worker.capabilities}
                            };
                        }
                    } else if (requestLine[1] == "/metrics") {
                        QJsonObject status = httpStatusProvider_ ? httpStatusProvider_() : QJsonObject();
                        int workerCount = 0;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            workerCount = static_cast<int>(workers_.size());
                        }
                        const auto metric = [&status](const QString& key, const QString& name) {
                            return name.toUtf8() + " " + QByteArray::number(status.value(key).toDouble());
                        };
                        metricsPayload = "# TYPE artifact_farm_workers gauge\n"
                            "artifact_farm_workers " + QByteArray::number(workerCount) + "\n"
                            "# TYPE artifact_farm_busy gauge\n"
                            "artifact_farm_busy " + QByteArray::number(status.value(QStringLiteral("busy")).toBool() ? 1 : 0) + "\n"
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
                        contentType = "text/plain; version=0.0.4";
                    } else {
                        statusCode = 404;
                        statusText = "Not Found";
                        body = {{QStringLiteral("error"), QStringLiteral("not_found")}};
                    }
                    const QByteArray payload = contentType.startsWith("text/")
                        ? metricsPayload : QJsonDocument(body).toJson(QJsonDocument::Compact);
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
