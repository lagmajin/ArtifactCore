module;
#include <utility>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

module Network.CollaborationWebSocket;

import std;

namespace ArtifactCore {

class CollaborationWebSocket::Impl {
public:
    QWebSocket* ws_ = nullptr;
    QString serverUrl_;
    QString projectId_;
    QString clientId_;
    CollabConnectionState state_ = CollabConnectionState::Disconnected;

    // Callbacks
    OperationReceivedCallback onOperationReceived_;
    PresenceUpdatedCallback onPresenceUpdated_;
    LockGrantedCallback onLockGranted_;
    LockDeniedCallback onLockDenied_;
    UserJoinedCallback onUserJoined_;
    UserLeftCallback onUserLeft_;
    ConnectionStateChangedCallback onConnectionStateChanged_;
    HistoryReceivedCallback onHistoryReceived_;

    Impl() {
        ws_ = new QWebSocket();
        QObject::connect(ws_, &QWebSocket::textMessageReceived,
            [this](const QString& message) {
                handleTextMessage(message);
            });
        QObject::connect(ws_, &QWebSocket::connected,
            [this]() {
                state_ = CollabConnectionState::Connected;
                if (onConnectionStateChanged_) onConnectionStateChanged_(state_);
                handleConnected();
            });
        QObject::connect(ws_, &QWebSocket::disconnected,
            [this]() {
                state_ = CollabConnectionState::Disconnected;
                if (onConnectionStateChanged_) onConnectionStateChanged_(state_);
                handleDisconnected();
            });
        // Note: QWebSocket does not have a direct error signal in all Qt versions
        // We rely on connection state changes and message handling
    }

    ~Impl() {
        if (ws_) {
            ws_->close();
            ws_->deleteLater();
            ws_ = nullptr;
        }
    }

    void handleTextMessage(const QString& message) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "[CollabWebSocket] Failed to parse message:" << message;
            return;
        }

        QJsonObject msg = doc.object();
        QString type = msg.value(QStringLiteral("type")).toString();

        if (type == QStringLiteral("operation")) {
            if (onOperationReceived_) onOperationReceived_(msg);
        } else if (type == QStringLiteral("presence")) {
            if (onPresenceUpdated_) onPresenceUpdated_(msg);
        } else if (type == QStringLiteral("lock_granted")) {
            if (onLockGranted_) onLockGranted_(msg.value(QStringLiteral("layerId")).toString());
        } else if (type == QStringLiteral("lock_denied")) {
            if (onLockDenied_) onLockDenied_(
                msg.value(QStringLiteral("layerId")).toString(),
                msg.value(QStringLiteral("reason")).toString());
        } else if (type == QStringLiteral("user_joined")) {
            if (onUserJoined_) {
                CollabUserInfo user;
                user.userId = msg.value(QStringLiteral("userId")).toString();
                user.userName = msg.value(QStringLiteral("userName")).toString();
                user.userColor = msg.value(QStringLiteral("userColor")).toString();
                onUserJoined_(user);
            }
        } else if (type == QStringLiteral("user_left")) {
            if (onUserLeft_) {
                CollabUserInfo user;
                user.userId = msg.value(QStringLiteral("userId")).toString();
                user.userName = msg.value(QStringLiteral("userName")).toString();
                user.userColor = msg.value(QStringLiteral("userColor")).toString();
                onUserLeft_(user);
            }
        } else if (type == QStringLiteral("history")) {
            if (onHistoryReceived_) {
                onHistoryReceived_(msg.value(QStringLiteral("operations")).toArray());
            }
        }
    }

    void handleConnected() {
        // Send join message
        QJsonObject joinMsg;
        joinMsg[QStringLiteral("type")] = QStringLiteral("join");
        joinMsg[QStringLiteral("projectId")] = projectId_;
        joinMsg[QStringLiteral("clientId")] = clientId_;
        ws_->sendTextMessage(QJsonDocument(joinMsg).toJson(QJsonDocument::Compact));
    }

    void handleDisconnected() {
        // Could implement auto-reconnect here
    }

    void handleError() {
        state_ = CollabConnectionState::Error;
        if (onConnectionStateChanged_) onConnectionStateChanged_(state_);
    }
};

CollaborationWebSocket::CollaborationWebSocket() : impl_(new Impl()) {}
CollaborationWebSocket::~CollaborationWebSocket() { delete impl_; }

void CollaborationWebSocket::connectToServer(const QString& serverUrl, const QString& projectId, const QString& clientId) {
    impl_->serverUrl_ = serverUrl;
    impl_->projectId_ = projectId;
    impl_->clientId_ = clientId;
    impl_->state_ = CollabConnectionState::Connecting;
    if (impl_->onConnectionStateChanged_) impl_->onConnectionStateChanged_(impl_->state_);
    impl_->ws_->open(QUrl(serverUrl));
}

void CollaborationWebSocket::disconnect() {
    impl_->ws_->close();
    impl_->state_ = CollabConnectionState::Disconnected;
    if (impl_->onConnectionStateChanged_) impl_->onConnectionStateChanged_(impl_->state_);
}

bool CollaborationWebSocket::isConnected() const {
    return impl_->state_ == CollabConnectionState::Connected;
}

CollabConnectionState CollaborationWebSocket::connectionState() const {
    return impl_->state_;
}

QString CollaborationWebSocket::serverUrl() const { return impl_->serverUrl_; }
QString CollaborationWebSocket::projectId() const { return impl_->projectId_; }
QString CollaborationWebSocket::clientId() const { return impl_->clientId_; }

void CollaborationWebSocket::sendOperation(const QJsonObject& operation) {
    if (!isConnected()) return;
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("operation");
    msg[QStringLiteral("clientId")] = impl_->clientId_;
    msg[QStringLiteral("projectId")] = impl_->projectId_;
    msg[QStringLiteral("operation")] = operation;
    msg[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    impl_->ws_->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void CollaborationWebSocket::sendPresence(const QJsonObject& presence) {
    if (!isConnected()) return;
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("presence");
    msg[QStringLiteral("clientId")] = impl_->clientId_;
    msg[QStringLiteral("presence")] = presence;
    msg[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    impl_->ws_->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void CollaborationWebSocket::sendLockRequest(const QString& layerId) {
    if (!isConnected()) return;
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("lock_request");
    msg[QStringLiteral("clientId")] = impl_->clientId_;
    msg[QStringLiteral("layerId")] = layerId;
    msg[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    impl_->ws_->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void CollaborationWebSocket::sendUnlockRequest(const QString& layerId) {
    if (!isConnected()) return;
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("unlock_request");
    msg[QStringLiteral("clientId")] = impl_->clientId_;
    msg[QStringLiteral("layerId")] = layerId;
    msg[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    impl_->ws_->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void CollaborationWebSocket::sendJoin(const QString& userId, const QString& userName, const QString& userColor) {
    if (!isConnected()) return;
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("join");
    msg[QStringLiteral("clientId")] = impl_->clientId_;
    msg[QStringLiteral("userId")] = userId;
    msg[QStringLiteral("userName")] = userName;
    msg[QStringLiteral("userColor")] = userColor;
    impl_->ws_->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void CollaborationWebSocket::handleTextMessage(const QString& message) { impl_->handleTextMessage(message); }
void CollaborationWebSocket::handleConnected() { impl_->handleConnected(); }
void CollaborationWebSocket::handleDisconnected() { impl_->handleDisconnected(); }
void CollaborationWebSocket::handleError() { impl_->handleError(); }

void CollaborationWebSocket::setOperationReceivedCallback(OperationReceivedCallback cb) { impl_->onOperationReceived_ = std::move(cb); }
void CollaborationWebSocket::setPresenceUpdatedCallback(PresenceUpdatedCallback cb) { impl_->onPresenceUpdated_ = std::move(cb); }
void CollaborationWebSocket::setLockGrantedCallback(LockGrantedCallback cb) { impl_->onLockGranted_ = std::move(cb); }
void CollaborationWebSocket::setLockDeniedCallback(LockDeniedCallback cb) { impl_->onLockDenied_ = std::move(cb); }
void CollaborationWebSocket::setUserJoinedCallback(UserJoinedCallback cb) { impl_->onUserJoined_ = std::move(cb); }
void CollaborationWebSocket::setUserLeftCallback(UserLeftCallback cb) { impl_->onUserLeft_ = std::move(cb); }
void CollaborationWebSocket::setConnectionStateChangedCallback(ConnectionStateChangedCallback cb) { impl_->onConnectionStateChanged_ = std::move(cb); }
void CollaborationWebSocket::setHistoryReceivedCallback(HistoryReceivedCallback cb) { impl_->onHistoryReceived_ = std::move(cb); }

} // namespace ArtifactCore
