module;
#include <memory>
#include <functional>
#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtNetwork/QTcpSocket>

module NetworkRPCClient;

namespace ArtifactCore {

class NetworkRPCClient::Impl {
public:
    QTcpSocket* socket_ = nullptr;
    QTimer* heartbeatTimer_ = nullptr;
    QString workerId_;
    bool connected_ = false;
    QByteArray readBuffer_;

    JobAssignedCallback onJobAssigned_;
    DisconnectedCallback onDisconnected_;

    // Heartbeat interval (same as server's check interval)
    static constexpr qint64 HEARTBEAT_INTERVAL_MS = 5000;

    Impl() {
        socket_ = new QTcpSocket();
        heartbeatTimer_ = new QTimer();
    }

    ~Impl() {
        disconnectInternal();
        delete heartbeatTimer_;
        delete socket_;
    }

    bool connectToServer(const QString& host, unsigned short port, const QString& workerId) {
        if (connected_) return false;
        workerId_ = workerId;

        QObject::connect(socket_, &QTcpSocket::connected, [this]() {
            sendRegistration();
        });

        QObject::connect(socket_, &QTcpSocket::readyRead, [this]() {
            onData();
        });

        QObject::connect(socket_, &QTcpSocket::disconnected, [this]() {
            connected_ = false;
            if (heartbeatTimer_) heartbeatTimer_->stop();
            if (onDisconnected_) onDisconnected_();
        });

        socket_->connectToHost(host, port);
        return socket_->waitForConnected(5000);
    }

    void disconnectInternal() {
        if (heartbeatTimer_) heartbeatTimer_->stop();
        if (socket_) {
            socket_->disconnectFromHost();
            if (socket_->state() != QAbstractSocket::UnconnectedState)
                socket_->waitForDisconnected(1000);
        }
        connected_ = false;
    }

    void sendRegistration() {
        QJsonObject params;
        params["workerId"] = workerId_;
        sendMessage(QStringLiteral("register"), params);
        connected_ = true;

        // Start heartbeat timer
        QObject::connect(heartbeatTimer_, &QTimer::timeout, [this]() {
            sendHeartbeat();
        });
        heartbeatTimer_->start(HEARTBEAT_INTERVAL_MS);
    }

    void sendHeartbeat() {
        if (!connected_) return;
        QJsonObject params;
        params["workerId"] = workerId_;
        sendMessage(QStringLiteral("heartbeat"), params);
    }

    void sendMessage(const QString& method, const QJsonObject& params) {
        if (!socket_ || !connected_) return;
        QJsonObject msg;
        msg["jsonrpc"] = "2.0";
        msg["method"] = method;
        msg["params"] = params;
        msg["id"] = 1;
        QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
        socket_->write(data);
        socket_->flush();
    }

    void onData() {
        readBuffer_.append(socket_->readAll());
        // Process complete JSON lines
        while (true) {
            int nl = readBuffer_.indexOf('\n');
            if (nl < 0) break;
            QByteArray line = readBuffer_.left(nl).trimmed();
            readBuffer_.remove(0, nl + 1);
            if (line.isEmpty()) continue;
            QJsonDocument doc = QJsonDocument::fromJson(line);
            if (doc.isNull() || !doc.isObject()) continue;
            handleMessage(doc.object());
        }
    }

    void handleMessage(const QJsonObject& msg) {
        QString method = msg["method"].toString();
        if (method == QStringLiteral("assignJob")) {
            QJsonObject params = msg["params"].toObject();
            if (onJobAssigned_) onJobAssigned_(params);
        }
        // Ignore RPC responses (result field) and other messages
    }

    bool sendFrameResult(const QString& method, int frame, const QString& error) {
        if (!connected_) return false;
        QJsonObject params;
        params["workerId"] = workerId_;
        params["frame"] = frame;
        if (!error.isEmpty()) params["error"] = error;
        sendMessage(method, params);
        return true;
    }
};

NetworkRPCClient::NetworkRPCClient()
    : impl_(new Impl())
{}

NetworkRPCClient::~NetworkRPCClient() {
    delete impl_;
}

bool NetworkRPCClient::connectToServer(const QString& host, unsigned short port, const QString& workerId) {
    return impl_->connectToServer(host, port, workerId);
}

void NetworkRPCClient::disconnect() {
    impl_->disconnectInternal();
}

bool NetworkRPCClient::isConnected() const {
    return impl_->connected_;
}

QString NetworkRPCClient::workerId() const {
    return impl_->workerId_;
}

void NetworkRPCClient::setOnJobAssigned(JobAssignedCallback cb) {
    impl_->onJobAssigned_ = std::move(cb);
}

void NetworkRPCClient::setOnDisconnected(DisconnectedCallback cb) {
    impl_->onDisconnected_ = std::move(cb);
}

bool NetworkRPCClient::sendFrameCompleted(int frame) {
    return impl_->sendFrameResult(QStringLiteral("frameCompleted"), frame, QString());
}

bool NetworkRPCClient::sendFrameFailed(int frame, const QString& error) {
    return impl_->sendFrameResult(QStringLiteral("frameFailed"), frame, error);
}

}
