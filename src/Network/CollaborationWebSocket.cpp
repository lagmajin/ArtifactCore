module;
#include <QWebSocket>
#include <QWebSocketProtocol>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <wobjectimpl.h>
#include <deque>
#include <functional>
#include <algorithm>
#include <cmath>

module Network.CollaborationWebSocket;

namespace ArtifactCore {

// ─── JSON helpers ───

QJsonObject JoinMessage::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("join");
    obj[QStringLiteral("projectId")] = projectId;
    obj[QStringLiteral("projectFingerprint")] = projectFingerprint;
    if (!accessToken.isEmpty()) {
        obj[QStringLiteral("accessToken")] = accessToken;
    }
    obj[QStringLiteral("clientId")] = clientId;
    obj[QStringLiteral("userId")] = userId;
    obj[QStringLiteral("userName")] = userName;
    obj[QStringLiteral("userColor")] = userColor;
    return obj;
}

QJsonObject OperationMessage::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("operation");
    obj[QStringLiteral("clientId")] = clientId;
    obj[QStringLiteral("projectId")] = projectId;
    obj[QStringLiteral("operation")] = operation;
    obj[QStringLiteral("version")] = version;
    obj[QStringLiteral("timestamp")] = timestamp;
    return obj;
}

QJsonObject LockRequestMessage::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("lock_request");
    obj[QStringLiteral("layerId")] = layerId;
    obj[QStringLiteral("clientId")] = clientId;
    return obj;
}

QJsonObject LockReleaseMessage::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("unlock_request");
    obj[QStringLiteral("layerId")] = layerId;
    obj[QStringLiteral("clientId")] = clientId;
    return obj;
}

QJsonObject PresenceMessage::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("presence");
    obj[QStringLiteral("clientId")] = clientId;
    obj[QStringLiteral("userId")] = userId;
    obj[QStringLiteral("userName")] = userName;
    obj[QStringLiteral("userColor")] = userColor;
    obj[QStringLiteral("presence")] = presence;
    return obj;
}

// ─── Impl ───

class CollaborationWebSocket::Impl {
public:
    QWebSocket ws;
    QTimer heartbeatTimer, reconnectTimer;
    CollabConnectionState state = CollabConnectionState::Disconnected;
    bool roomReady = false;
    bool readOnly = false;
    JoinMessage joinInfo;
    QString serverUrl;
    QString sessionId;
    std::deque<std::function<void()>> pendingQueue;
    std::function<void(CollabConnectionState)> connectionStateCallback;
    std::function<void()> roomReadyCallback;
    std::function<void(const QString&)> protocolErrorCallback;
    std::function<void(const QString&, qint64, const QString&)>
        operationRejectedCallback;
    int reconnectAttempt = 0;
    static constexpr int kMaxQueue = 256, kMaxReconnect = 6;
    static constexpr int kReconnectBaseMs = 1000, kReconnectCapMs = 30000, kHeartbeatMs = 25000;

    void setState(CollabConnectionState s, CollaborationWebSocket* self) {
        if (state != s) {
            state = s;
            if (connectionStateCallback) connectionStateCallback(s);
            Q_EMIT self->connectionStateChanged(s);
        }
    }
    void flushQueue() {
        while (!pendingQueue.empty()) {
            pendingQueue.front()();
            pendingQueue.pop_front();
        }
    }
    void reportProtocolError(const QString& reason,
                             CollaborationWebSocket* self) {
        if (protocolErrorCallback) protocolErrorCallback(reason);
        Q_EMIT self->protocolError(reason);
    }
    bool enqueue(std::function<qint64()> send) {
        if (static_cast<int>(pendingQueue.size()) >= kMaxQueue) {
            return false;
        }
        pendingQueue.push_back([send = std::move(send)]() {
            if (send() < 0) {
                qWarning() << "[CollaborationWebSocket] Queued message send failed";
            }
        });
        return true;
    }
    bool sendOrQueue(CollaborationWebSocket* self,
                     std::function<qint64()> send) {
        if (state != CollabConnectionState::Connected) {
            if (enqueue(std::move(send))) return true;
            const QString reason = QStringLiteral("Outbound collaboration queue is full");
            qWarning() << "[CollaborationWebSocket]" << reason;
            reportProtocolError(reason, self);
            return false;
        }
        if (send() >= 0) return true;
        const QString reason = QStringLiteral("Could not send collaboration message");
        qWarning() << "[CollaborationWebSocket]" << reason;
        reportProtocolError(reason, self);
        return false;
    }
    void scheduleReconnect(CollaborationWebSocket* self) {
        if (reconnectAttempt >= kMaxReconnect) {
            setState(CollabConnectionState::Error, self); return;
        }
        setState(CollabConnectionState::Reconnecting, self);
        int delay = std::min(kReconnectBaseMs * (1 << reconnectAttempt), kReconnectCapMs);
        reconnectTimer.start(delay);
    }
};

W_OBJECT_IMPL(CollaborationWebSocket)

CollaborationWebSocket::CollaborationWebSocket(QObject* parent)
    : QObject(parent), impl_(new Impl())
{
    impl_->sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* s = this;
    QObject::connect(&impl_->ws, &QWebSocket::connected, s, [s, this]() {
        impl_->roomReady = false;
        impl_->readOnly = false;
        impl_->reconnectAttempt = 0; impl_->reconnectTimer.stop();
        impl_->setState(CollabConnectionState::Connected, s);
        QJsonDocument doc(impl_->joinInfo.toJson());
        impl_->ws.sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        impl_->flushQueue();
        impl_->heartbeatTimer.start(Impl::kHeartbeatMs);
    });
    QObject::connect(&impl_->ws, &QWebSocket::disconnected, s, [s, this]() {
        impl_->roomReady = false;
        impl_->heartbeatTimer.stop();
        if (impl_->ws.closeCode() ==
            QWebSocketProtocol::CloseCodePolicyViolated) {
            impl_->reconnectTimer.stop();
            impl_->setState(CollabConnectionState::Error, s);
            return;
        }
        if (impl_->state == CollabConnectionState::Connected ||
            impl_->state == CollabConnectionState::Reconnecting)
            impl_->scheduleReconnect(s);
    });
    QObject::connect(&impl_->ws, &QWebSocket::textMessageReceived, s,
        [s, this](const QString& msg) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError) {
                impl_->reportProtocolError(
                    QStringLiteral("Invalid JSON: %1").arg(err.errorString()), s);
                return;
            }
            if (!doc.isObject()) return;
            QJsonObject o = doc.object();
            QString t = o.value(QStringLiteral("type")).toString();
            if (t == QStringLiteral("room_ready")) {
                if (!impl_->roomReady) {
                    impl_->readOnly =
                        o.value(QStringLiteral("role")).toString() ==
                        QStringLiteral("viewer");
                    impl_->roomReady = true;
                    if (impl_->roomReadyCallback) {
                        impl_->roomReadyCallback();
                    }
                }
            } else if (t == QStringLiteral("operation")) {
                OperationMessage op;
                op.clientId = o.value(QStringLiteral("clientId")).toString();
                op.operation = o.value(QStringLiteral("operation")).toObject();
                op.version = o.value(QStringLiteral("version")).toInt();
                Q_EMIT s->remoteOperation(op);
            } else if (t == QStringLiteral("lock_granted")) {
                Q_EMIT s->remoteLockGranted(o.value(QStringLiteral("layerId")).toString(),
                    o.value(QStringLiteral("clientId")).toString(),
                    o.value(QStringLiteral("userId")).toString());
            } else if (t == QStringLiteral("lock_updated")) {
                Q_EMIT s->remoteLockGranted(o.value(QStringLiteral("layerId")).toString(),
                    o.value(QStringLiteral("clientId")).toString(),
                    o.value(QStringLiteral("userId")).toString());
            } else if (t == QStringLiteral("lock_denied")) {
                Q_EMIT s->remoteLockDenied(o.value(QStringLiteral("layerId")).toString(),
                    o.value(QStringLiteral("reason")).toString());
            } else if (t == QStringLiteral("lock_released")) {
                Q_EMIT s->remoteLockReleased(o.value(QStringLiteral("layerId")).toString(),
                    o.value(QStringLiteral("clientId")).toString());
            } else if (t == QStringLiteral("presence")) {
                PresenceMessage p;
                p.clientId = o.value(QStringLiteral("clientId")).toString();
                p.userId = o.value(QStringLiteral("userId")).toString();
                p.userName = o.value(QStringLiteral("userName")).toString();
                p.userColor = o.value(QStringLiteral("userColor")).toString();
                p.presence = o.value(QStringLiteral("presence")).toObject();
                Q_EMIT s->remotePresence(p);
            } else if (t == QStringLiteral("user_joined")) {
                Q_EMIT s->userJoined(o.value(QStringLiteral("clientId")).toString(),
                    o.value(QStringLiteral("userId")).toString(),
                    o.value(QStringLiteral("userName")).toString(),
                    o.value(QStringLiteral("userColor")).toString());
            } else if (t == QStringLiteral("user_left")) {
                Q_EMIT s->userLeft(o.value(QStringLiteral("clientId")).toString(),
                    o.value(QStringLiteral("userId")).toString(),
                    o.value(QStringLiteral("userName")).toString());
            } else if (t == QStringLiteral("history")) {
                QJsonArray ops = o.value(QStringLiteral("operations")).toArray();
                for (const QJsonValue& v : ops) {
                    if (!v.isObject()) continue;
                    QJsonObject ho = v.toObject();
                    OperationMessage op;
                    op.clientId = ho.value(QStringLiteral("clientId")).toString();
                    op.operation = ho.value(QStringLiteral("operation")).toObject();
                    op.version = ho.value(QStringLiteral("version")).toInt();
                    Q_EMIT s->remoteOperation(op);
                }
            } else if (t == QStringLiteral("error")) {
                const QString message =
                    o.value(QStringLiteral("message")).toString();
                const QJsonValue rejectedSequence =
                    o.value(QStringLiteral("opSeq"));
                impl_->reportProtocolError(message, s);
                if (rejectedSequence.isDouble() &&
                    std::isfinite(rejectedSequence.toDouble()) &&
                    std::floor(rejectedSequence.toDouble()) ==
                        rejectedSequence.toDouble() &&
                    rejectedSequence.toDouble() >= 0.0 &&
                    rejectedSequence.toDouble() <= 9007199254740991.0 &&
                    impl_->operationRejectedCallback) {
                    impl_->operationRejectedCallback(
                        o.value(QStringLiteral("clientId")).toString(),
                        static_cast<qint64>(rejectedSequence.toDouble()),
                        message);
                }
            // --- Rule sync (Collaborate.Protocol) ---
            } else if (t == QStringLiteral("rule_added")) {
                Q_EMIT s->ruleAdded(o.value(QStringLiteral("ruleId")).toString(),
                    QString::fromUtf8(QJsonDocument(o.value(QStringLiteral("payload")).toObject()).toJson(QJsonDocument::Compact)));
            } else if (t == QStringLiteral("rule_removed")) {
                Q_EMIT s->ruleRemoved(o.value(QStringLiteral("ruleId")).toString());
            } else if (t == QStringLiteral("rule_updated")) {
                Q_EMIT s->ruleUpdated(o.value(QStringLiteral("ruleId")).toString(),
                    QString::fromUtf8(QJsonDocument(o.value(QStringLiteral("payload")).toObject()).toJson(QJsonDocument::Compact)));
            } else if (t == QStringLiteral("rule_executed")) {
                Q_EMIT s->ruleExecuted(o.value(QStringLiteral("ruleId")).toString());
            }
        });
    impl_->heartbeatTimer.setSingleShot(false);
    QObject::connect(&impl_->heartbeatTimer, &QTimer::timeout, s, [s, this]() {
        if (impl_->state == CollabConnectionState::Connected) {
            QJsonObject ping;
            ping[QStringLiteral("type")] = QStringLiteral("ping");
            ping[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
            impl_->ws.sendTextMessage(
                QString::fromUtf8(QJsonDocument(ping).toJson(QJsonDocument::Compact)));
        }
    });
    impl_->reconnectTimer.setSingleShot(true);
    QObject::connect(&impl_->reconnectTimer, &QTimer::timeout, s, [s, this]() {
        impl_->reconnectAttempt++;
        impl_->setState(CollabConnectionState::Connecting, s);
        impl_->ws.open(QUrl(impl_->serverUrl));
    });
}


CollaborationWebSocket::~CollaborationWebSocket() { delete impl_; }

void CollaborationWebSocket::connectToServer(const QString& url, const JoinMessage& join) {
    impl_->roomReady = false;
    impl_->serverUrl = url; impl_->joinInfo = join; impl_->reconnectAttempt = 0;
    impl_->setState(CollabConnectionState::Connecting, this);
    impl_->ws.open(QUrl(url));
}

void CollaborationWebSocket::disconnect() {
    impl_->roomReady = false;
    impl_->reconnectTimer.stop(); impl_->heartbeatTimer.stop();
    impl_->ws.close(); impl_->setState(CollabConnectionState::Disconnected, this);
}

bool CollaborationWebSocket::isConnected() const {
    return impl_->state == CollabConnectionState::Connected;
}

bool CollaborationWebSocket::isRoomReady() const {
    return impl_->state == CollabConnectionState::Connected && impl_->roomReady;
}

bool CollaborationWebSocket::isReadOnly() const {
    return impl_->readOnly;
}

CollabConnectionState CollaborationWebSocket::connectionState() const {
    return impl_->state;
}

void CollaborationWebSocket::setConnectionStateCallback(
    std::function<void(CollabConnectionState)> callback) {
    impl_->connectionStateCallback = std::move(callback);
}

void CollaborationWebSocket::setRoomReadyCallback(
    std::function<void()> callback) {
    impl_->roomReadyCallback = std::move(callback);
}

void CollaborationWebSocket::setProtocolErrorCallback(
    std::function<void(const QString&)> callback) {
    impl_->protocolErrorCallback = std::move(callback);
}

void CollaborationWebSocket::setOperationRejectedCallback(
    std::function<void(const QString&, qint64, const QString&)> callback) {
    impl_->operationRejectedCallback = std::move(callback);
}

bool CollaborationWebSocket::sendOperation(const OperationMessage& op) {
    return impl_->sendOrQueue(this, [this, op]() {
        return impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(op.toJson()).toJson(QJsonDocument::Compact)));
    });
}

bool CollaborationWebSocket::sendLockRequest(const LockRequestMessage& req) {
    return impl_->sendOrQueue(this, [this, req]() {
        return impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(req.toJson()).toJson(QJsonDocument::Compact)));
    });
}

bool CollaborationWebSocket::sendLockRelease(const LockReleaseMessage& rel) {
    return impl_->sendOrQueue(this, [this, rel]() {
        return impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(rel.toJson()).toJson(QJsonDocument::Compact)));
    });
}

bool CollaborationWebSocket::sendPresence(const PresenceMessage& pres) {
    return impl_->sendOrQueue(this, [this, pres]() {
        return impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(pres.toJson()).toJson(QJsonDocument::Compact)));
    });
}

void CollaborationWebSocket::sendRuleSync(const QString& type, const QString& ruleId, const QString& payload) {
    impl_->sendOrQueue(this, [this, type, ruleId, payload]() {
        QJsonObject obj;
        obj[QStringLiteral("type")] = type;
        obj[QStringLiteral("ruleId")] = ruleId;
        obj[QStringLiteral("sessionId")] = impl_->sessionId;
        if (!payload.isEmpty()) {
            obj[QStringLiteral("payload")] = QJsonDocument::fromJson(payload.toUtf8()).object();
        }
        return impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    });
}

QString CollaborationWebSocket::sessionId() const {
    return impl_->sessionId;
}

} // namespace ArtifactCore
