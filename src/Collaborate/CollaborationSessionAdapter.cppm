module;

#include <QObject>
#include <QMetaObject>
#include <QDateTime>
#include <QString>
#include <QJsonObject>

export module Collaborate.SessionAdapter;

import Network.CollaborationWebSocket;
import Collaborate.Session;
import Collaborate.Operations;
import Core.ArtifactArray;

export namespace ArtifactCore {

// Point-to-point glue between one CollaborationWebSocket and one
// CollaborationSession: inbound signals are routed into the session model,
// outbound session intents are forwarded to the socket. No global event
// wiring; connections are scoped to this adapter and dropped in the
// destructor. The websocket and session references must outlive the adapter.
class CollaborationSessionAdapter {
public:
    CollaborationSessionAdapter(CollaborationWebSocket& webSocket,
                                CollaborationSession& session,
                                const QString& projectId)
        : ws_(webSocket), session_(session), projectId_(projectId) {
        connectAll();
    }

    ~CollaborationSessionAdapter() {
        for (const auto& connection : connections_) {
            QObject::disconnect(connection);
        }
    }

    CollaborationSessionAdapter(const CollaborationSessionAdapter&) = delete;
    CollaborationSessionAdapter& operator=(const CollaborationSessionAdapter&) = delete;

    [[nodiscard]] bool connected() const noexcept { return ws_.isConnected(); }
    [[nodiscard]] CollabConnectionState connectionState() const noexcept {
        return ws_.connectionState();
    }
    [[nodiscard]] const QString& projectId() const noexcept { return projectId_; }

    // ---- outbound (session intent -> transport) ----

    void sendLocalOperation(const CollabOperationData& operation) {
        OperationMessage message;
        message.clientId = operation.clientId;
        message.projectId = projectId_;
        message.operation = operation.toJson();
        message.version = static_cast<int>(operation.version);
        ws_.sendOperation(message);
    }

    void sendLocalLockRequest(const QString& layerId) {
        LockRequestMessage request;
        request.layerId = layerId;
        request.clientId = session_.localClientId();
        ws_.sendLockRequest(request);
    }

    void sendLocalLockRelease(const QString& layerId) {
        LockReleaseMessage release;
        release.layerId = layerId;
        release.clientId = session_.localClientId();
        ws_.sendLockRelease(release);
    }

    void sendLocalPresence(const CollabPresenceState& presence) {
        PresenceMessage message;
        message.clientId = session_.localClientId();
        message.userId = session_.localIdentity().userId;
        message.userName = session_.localIdentity().userName;
        message.userColor = session_.localIdentity().userColor;
        message.presence = presence.toJson();
        ws_.sendPresence(message);
    }

private:
    void connectAll() {
        auto* receiver = &ws_;

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteOperation, receiver,
            [this](const OperationMessage& op) {
                CollabOperationData operation;
                operation.type =
                    op.operation.value(QStringLiteral("type")).toString();
                operation.layerId =
                    op.operation.value(QStringLiteral("layerId")).toString();
                operation.payload = op.operation.value(QStringLiteral("payload")).toObject();
                operation.version = op.version;
                operation.clientId = op.clientId;
                operation.sequence =
                    op.operation.contains(QStringLiteral("opSeq"))
                        ? op.operation.value(QStringLiteral("opSeq"))
                              .toVariant().toLongLong()
                        : -1;
                operation.timestampMs =
                    op.operation.contains(QStringLiteral("clientTimestamp"))
                        ? op.operation.value(QStringLiteral("clientTimestamp"))
                              .toVariant().toLongLong()
                        : 0;
                session_.processRemoteOperation(operation);
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::userJoined, receiver,
            [this](const QString& clientId, const QString& userId,
                   const QString& userName) {
                // Color arrives through the follow-up presence broadcast.
                session_.processUserJoined(clientId, userId, userName, {},
                                           QDateTime::currentMSecsSinceEpoch());
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::userLeft, receiver,
            [this](const QString& clientId, const QString&, const QString&) {
                session_.processUserLeft(clientId);
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remotePresence, receiver,
            [this](const PresenceMessage& pres) {
                session_.processPresence(pres.clientId, pres.presence,
                                         QDateTime::currentMSecsSinceEpoch());
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteLockGranted, receiver,
            [this](const QString& layerId, const QString& clientId,
                   const QString& byUserId) {
                session_.processLockGranted(layerId, clientId, byUserId, {},
                                            QDateTime::currentMSecsSinceEpoch());
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteLockReleased, receiver,
            [this](const QString& layerId, const QString&) {
                session_.processLockReleased(layerId);
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteLockDenied, receiver,
            [this](const QString& layerId, const QString& reason) {
                session_.processLockDenied(layerId, reason);
            }));
    }

    CollaborationWebSocket& ws_;
    CollaborationSession& session_;
    QString projectId_;
    Array<QMetaObject::Connection> connections_;
};

} // namespace ArtifactCore
