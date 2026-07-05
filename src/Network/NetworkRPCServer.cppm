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
#include <QDateTime>
#include <QDebug>

module NetworkRPCServer;

namespace ArtifactCore {

class NetworkPCServer::Impl {
public:
    QTcpServer* server_ = nullptr;
    unsigned short port_ = 0;
    bool running_ = false;

    std::map<QTcpSocket*, RemoteWorkerInfo> workers_;
    std::map<QString, QTcpSocket*> workerSockets_;
    mutable std::mutex mutex_;

    WorkerConnectedCallback onWorkerConnected_;
    WorkerDisconnectedCallback onWorkerDisconnected_;
    WorkerHeartbeatCallback onWorkerHeartbeat_;
    RpcRequestHandler onRequest_;

    // Heartbeat: timer-based dead detection via QObject::connect + singleShot chain
    static constexpr qint64 HEARTBEAT_TIMEOUT_MS = 30000;
    static constexpr qint64 HEARTBEAT_CHECK_INTERVAL_MS = 5000;

    Impl() {
        server_ = new QTcpServer();
    }

    ~Impl() {
        stopInternal();
        delete server_;
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
        QByteArray data = socket->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) return;

        QJsonObject msg = doc.object();
        QString method = msg["method"].toString();

        if (method == "register") {
            handleRegister(socket, msg);
        }
        else if (method == "heartbeat") {
            handleHeartbeat(socket, msg);
        }
        else {
            handleRpc(socket, msg);
        }
    }

    void handleRegister(QTcpSocket* socket, const QJsonObject& msg) {
        QString workerId = msg["params"].toObject()["workerId"].toString();
        if (workerId.isEmpty()) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = workers_.find(socket);
            if (it != workers_.end()) {
                it->second.workerId = workerId;
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
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = workerSockets_.find(workerId);
            if (it != workerSockets_.end()) {
                auto wit = workers_.find(it->second);
                if (wit != workers_.end())
                    wit->second.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
            }
            if (onWorkerHeartbeat_)
                onWorkerHeartbeat_(workerId);
        }
    }

    void handleRpc(QTcpSocket* socket, const QJsonObject& msg) {
        QString method = msg["method"].toString();
        QJsonObject params = msg["params"].toObject();
        QJsonObject result;
        if (onRequest_)
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
            QString wid;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = workers_.find(sock);
                if (it != workers_.end()) wid = it->second.workerId;
            }
            qDebug() << "[Farm] Heartbeat timeout:" << wid;
            sock->disconnectFromHost();
            if (!wid.isEmpty() && onWorkerDisconnected_)
                onWorkerDisconnected_(wid);
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
        }
        if (server_->isListening())
            server_->close();
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
    msg["jsonrpc"] = "2.0"; msg["method"] = method; msg["params"] = params; msg["id"] = 1;
    impl_->sendJson(s, msg);
    return "{}";
}

bool NetworkPCServer::sendJobAssignment(const QString& wid, const QJsonObject& jobJson) {
    QTcpSocket* s = impl_->findSocket(wid);
    if (!s) return false;
    QJsonObject msg;
    msg["jsonrpc"] = "2.0"; msg["method"] = "assignJob"; msg["params"] = jobJson; msg["id"] = 1;
    impl_->sendJson(s, msg);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        auto it = impl_->workerSockets_.find(wid);
        if (it != impl_->workerSockets_.end()) {
            auto wit = impl_->workers_.find(it->second);
            if (wit != impl_->workers_.end())
                wit->second.assignedFrames++;
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
