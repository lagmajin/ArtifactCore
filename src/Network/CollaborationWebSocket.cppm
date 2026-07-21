module;
#include <QWebSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
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
    JoinMessage joinInfo;
    QString serverUrl;
    std::deque<std::function<void()>> pendingQueue;
    int reconnectAttempt = 0;
    static constexpr int kMaxQueue = 256, kMaxReconnect = 6;
    static constexpr int kReconnectBaseMs = 1000, kReconnectCapMs = 30000, kHeartbeatMs = 25000;

    void setState(CollabConnectionState s, CollaborationWebSocket* self) {
        if (state != s) { state = s; Q_EMIT self->connectionStateChanged(s); }
    }
    void flushQueue() {
        while (!pendingQueue.empty()) { pendingQueue.front()(); pendingQueue.pop_front(); }
    }
    void enqueue(std::function<void()> fn) {
        if ((int)pendingQueue.size() >= kMaxQueue) pendingQueue.pop_front();
        pendingQueue.push_back(std::move(fn));
    }
    void sendOrQueue(CollaborationWebSocket* self, std::function<void()> fn) {
        if (state == CollabConnectionState::Connected) fn();
        else enqueue(std::move(fn));
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
    auto* s = this;
    QObject::connect(&impl_->ws, &QWebSocket::connected, s, [s, this]() {
        impl_->reconnectAttempt = 0; impl_->reconnectTimer.stop();
        impl_->setState(CollabConnectionState::Connected, s);
        QJsonDocument doc(impl_->joinInfo.toJson());
        impl_->ws.sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        impl_->flushQueue();
        impl_->heartbeatTimer.start(Impl::kHeartbeatMs);
    });
    QObject::connect(&impl_->ws, &QWebSocket::disconnected, s, [s, this]() {
        impl_->heartbeatTimer.stop();
        if (impl_->state == CollabConnectionState::Connected ||
            impl_->state == CollabConnectionState::Reconnecting)
            impl_->scheduleReconnect(s);
    });
    QObject::connect(&impl_->ws, &QWebSocket::textMessageReceived, s,
        [s, this](const QString& msg) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError) {
                Q_EMIT s->protocolError(QStringLiteral("Invalid JSON: %1").arg(err.errorString()));
                return;
            }
            if (!doc.isObject()) return;
            QJsonObject o = doc.object();
            QString t = o.value(QStringLiteral("type")).toString();
            if (t == QStringLiteral("operation")) {
                OperationMessage op;
                op.clientId = o.value(QStringLiteral("clientId")).toString();
                op.operation = o.value(QStringLiteral("operation")).toObject();
                op.version = o.value(QStringLiteral("version")).toInt();
                Q_EMIT s->remoteOperation(op);
            } else if (t == QStringLiteral("lock_granted")) {
                Q_EMIT s->remoteLockGranted(o.value(QStringLiteral("layerId")).toString(),
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
                Q_EMIT s->userJoined(o.value(QStringLiteral("userId")).toString(),
                    o.value(QStringLiteral("userName")).toString());
            } else if (t == QStringLiteral("user_left")) {
                Q_EMIT s->userLeft(o.value(QStringLiteral("userId")).toString(),
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
                Q_EMIT s->protocolError(o.value(QStringLiteral("message")).toString());
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
    impl_->serverUrl = url; impl_->joinInfo = join; impl_->reconnectAttempt = 0;
    impl_->setState(CollabConnectionState::Connecting, this);
    impl_->ws.open(QUrl(url));
}

void CollaborationWebSocket::disconnect() {
    impl_->reconnectTimer.stop(); impl_->heartbeatTimer.stop();
    impl_->ws.close(); impl_->setState(CollabConnectionState::Disconnected, this);
}

bool CollaborationWebSocket::isConnected() const {
    return impl_->state == CollabConnectionState::Connected;
}

CollabConnectionState CollaborationWebSocket::connectionState() const {
    return impl_->state;
}

void CollaborationWebSocket::sendOperation(const OperationMessage& op) {
    impl_->sendOrQueue(this, [this, op]() {
        impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(op.toJson()).toJson(QJsonDocument::Compact)));
    });
}

void CollaborationWebSocket::sendLockRequest(const LockRequestMessage& req) {
    impl_->sendOrQueue(this, [this, req]() {
        impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(req.toJson()).toJson(QJsonDocument::Compact)));
    });
}

void CollaborationWebSocket::sendLockRelease(const LockReleaseMessage& rel) {
    impl_->sendOrQueue(this, [this, rel]() {
        impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(rel.toJson()).toJson(QJsonDocument::Compact)));
    });
}

void CollaborationWebSocket::sendPresence(const PresenceMessage& pres) {
    impl_->sendOrQueue(this, [this, pres]() {
        impl_->ws.sendTextMessage(
            QString::fromUtf8(QJsonDocument(pres.toJson()).toJson(QJsonDocument::Compact)));
    });
}

} // namespace ArtifactCore
