module;
#include <memory>
#include <algorithm>
#include <functional>
#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslCertificate>
#include <QFile>
#include <cstdint>

module NetworkRPCClient;
import NetworkRPCClient;

namespace ArtifactCore {

class NetworkRPCClient::Impl {
public:
    QSslSocket* socket_ = nullptr;
    QTimer* heartbeatTimer_ = nullptr;
    QString workerId_;
    QString authToken_;
    QJsonObject capabilities_;
    bool connected_ = false;
    bool signalConnectionsInstalled_ = false;
    bool heartbeatConnectionInstalled_ = false;
    QByteArray readBuffer_;
    std::uint64_t nextRpcId_ = 1;
    bool tlsEnabled_ = false;
    QString caCertificateFile_;

    JobAssignedCallback onJobAssigned_;
    DisconnectedCallback onDisconnected_;

    // Heartbeat interval (same as server's check interval)
    static constexpr qint64 HEARTBEAT_INTERVAL_MS = 5000;

    Impl() {
        socket_ = new QSslSocket();
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
        readBuffer_.clear();

        if (!signalConnectionsInstalled_) {
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
            signalConnectionsInstalled_ = true;
        }

        if (tlsEnabled_) {
            if (!caCertificateFile_.isEmpty()) {
                QFile caFile(caCertificateFile_);
                if (caFile.open(QIODevice::ReadOnly)) {
                    socket_->setCaCertificates(QSslCertificate::fromData(
                        caFile.readAll(), QSsl::Pem));
                }
            }
            socket_->connectToHostEncrypted(host, port);
        } else {
            socket_->connectToHost(host, port);
        }
        return tlsEnabled_ ? socket_->waitForEncrypted(5000)
                           : socket_->waitForConnected(5000);
    }

    void disconnectInternal() {
        if (heartbeatTimer_) heartbeatTimer_->stop();
        if (socket_) {
            socket_->disconnectFromHost();
            if (socket_->state() != QAbstractSocket::UnconnectedState)
                socket_->waitForDisconnected(1000);
        }
        connected_ = false;
        readBuffer_.clear();
    }

    void sendRegistration() {
        QJsonObject params;
        params["workerId"] = workerId_;
        if (!authToken_.isEmpty()) params["authToken"] = authToken_;
        params["capabilities"] = capabilities_;
        sendMessage(QStringLiteral("register"), params);
        connected_ = true;

        // Start heartbeat timer
        if (!heartbeatConnectionInstalled_) {
            QObject::connect(heartbeatTimer_, &QTimer::timeout, [this]() {
                sendHeartbeat();
            });
            heartbeatConnectionInstalled_ = true;
        }
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
        msg["id"] = static_cast<qint64>(nextRpcId_++);
        QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
        socket_->write(data);
        socket_->flush();
    }

    void onData() {
        readBuffer_.append(socket_->readAll());
        constexpr qsizetype kMaxRpcMessageBytes = 16 * 1024 * 1024;
        if (readBuffer_.size() > kMaxRpcMessageBytes) {
            disconnectInternal();
            return;
        }
        // Process complete JSON lines
        while (true) {
            const qsizetype nl = readBuffer_.indexOf('\n');
            if (nl < 0) break;
            QByteArray line = readBuffer_.left(nl).trimmed();
            readBuffer_.remove(0, nl + 1);
            if (line.isEmpty()) continue;
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) continue;
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

void NetworkRPCClient::setAuthToken(const QString& token) {
    impl_->authToken_ = token;
}

void NetworkRPCClient::setCapabilities(const QJsonObject& capabilities) {
    impl_->capabilities_ = capabilities;
}

void NetworkRPCClient::setTlsEnabled(bool enabled, const QString& caCertificateFile) {
    impl_->tlsEnabled_ = enabled;
    impl_->caCertificateFile_ = caCertificateFile;
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

bool NetworkRPCClient::sendWorkerProgress(int completedFrames, int failedFrames, int currentFrame,
                                          qint64 renderTimeMs) {
    if (!impl_->connected_) return false;
    QJsonObject params;
    params[QStringLiteral("workerId")] = impl_->workerId_;
    params[QStringLiteral("completedFrames")] = std::max(0, completedFrames);
    params[QStringLiteral("failedFrames")] = std::max(0, failedFrames);
    params[QStringLiteral("currentFrame")] = currentFrame;
    params[QStringLiteral("renderTimeMs")] = std::max<qint64>(0, renderTimeMs);
    impl_->sendMessage(QStringLiteral("workerProgress"), params);
    return true;
}

bool NetworkRPCClient::sendWorkerLog(const QString& severity, const QString& message, int frame) {
    if (!impl_->connected_) return false;
    QJsonObject params;
    params[QStringLiteral("workerId")] = impl_->workerId_;
    params[QStringLiteral("severity")] = severity;
    params[QStringLiteral("message")] = message;
    params[QStringLiteral("frame")] = frame;
    impl_->sendMessage(QStringLiteral("workerLog"), params);
    return true;
}

}
