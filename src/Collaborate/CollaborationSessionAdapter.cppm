module;

#include <QObject>
#include <QMetaObject>
#include <QDateTime>
#include <QString>
#include <QJsonObject>
#include <functional>
#include <utility>

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

    void setParticipantsChangedCallback(std::function<void()> callback) {
        participantsChangedCallback_ = std::move(callback);
    }

    void setLockStateChangedCallback(
        std::function<void(const QString&)> callback) {
        lockStateChangedCallback_ = std::move(callback);
    }

    void setOperationAppliedCallback(
        std::function<void(const CollabOperationData&)> callback) {
        operationAppliedCallback_ = std::move(callback);
    }

    void setRemoteOperationValidator(
        std::function<bool(const CollabOperationData&)> validator) {
        remoteOperationValidator_ = std::move(validator);
    }

    // ---- outbound (session intent -> transport) ----

    [[nodiscard]] bool sendLocalOperation(const CollabOperationData& operation) {
        if (!validateCollabOperation(operation).isEmpty() ||
            operation.clientId.isEmpty() ||
            operation.clientId != session_.localClientId()) {
            (void)session_.discardPendingLocalOperation(operation);
            return false;
        }
        // Durable edits are never parked in the best-effort reconnect queue,
        // and must wait until the room's history and lock snapshot are ready.
        // The caller must keep the draft and retry after room_ready.
        if (!ws_.isRoomReady() || ws_.isReadOnly()) {
            (void)session_.discardPendingLocalOperation(operation);
            return false;
        }
        OperationMessage message;
        message.clientId = operation.clientId;
        message.projectId = projectId_;
        message.operation = operation.toJson();
        message.version = static_cast<int>(operation.version);
        const bool sent = ws_.sendOperation(message);
        if (!sent) (void)session_.discardPendingLocalOperation(operation);
        return sent;
    }

    [[nodiscard]] bool sendLocalLockRequest(const QString& layerId) {
        if (!ws_.isRoomReady() || ws_.isReadOnly() || layerId.isEmpty()) return false;
        LockRequestMessage request;
        request.layerId = layerId;
        request.clientId = session_.localClientId();
        return ws_.sendLockRequest(request);
    }

    [[nodiscard]] bool sendLocalLockRelease(const QString& layerId) {
        if (!ws_.isRoomReady() || ws_.isReadOnly() || layerId.isEmpty()) return false;
        LockReleaseMessage release;
        release.layerId = layerId;
        release.clientId = session_.localClientId();
        return ws_.sendLockRelease(release);
    }

    [[nodiscard]] bool sendLocalPresence(const CollabPresenceState& presence) {
        if (!ws_.isConnected()) return false;
        PresenceMessage message;
        message.clientId = session_.localClientId();
        message.userId = session_.localIdentity().userId;
        message.userName = session_.localIdentity().userName;
        message.userColor = session_.localIdentity().userColor;
        message.presence = presence.toJson();
        return ws_.sendPresence(message);
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
                if (operation.clientId.isEmpty() ||
                    operation.clientId != op.clientId ||
                    !validateCollabOperation(operation).isEmpty()) {
                    return;
                }
                const bool wasPendingLocal =
                    session_.isPendingLocalOperation(operation);
                if (!session_.hasSeenOperation(operation) &&
                    remoteOperationValidator_ &&
                    !remoteOperationValidator_(operation)) {
                    return;
                }
                const bool acceptedNew =
                    session_.processRemoteOperation(operation);
                if ((acceptedNew || wasPendingLocal) && operationAppliedCallback_) {
                    operationAppliedCallback_(operation);
                }
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::userJoined, receiver,
            [this](const QString& clientId, const QString& userId,
                   const QString& userName, const QString& userColor) {
                session_.processUserJoined(clientId, userId, userName, userColor,
                                           QDateTime::currentMSecsSinceEpoch());
                notifyParticipantsChanged();
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::userLeft, receiver,
            [this](const QString& clientId, const QString&, const QString&) {
                const auto releasedLocks = session_.processUserLeft(clientId);
                for (const QString& layerId : releasedLocks) {
                    notifyLockStateChanged(layerId);
                }
                notifyParticipantsChanged();
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remotePresence, receiver,
            [this](const PresenceMessage& pres) {
                session_.processPresence(
                    pres.clientId, pres.userId, pres.userName, pres.userColor,
                    pres.presence, QDateTime::currentMSecsSinceEpoch());
                notifyParticipantsChanged();
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteLockGranted, receiver,
            [this](const QString& layerId, const QString& clientId,
                   const QString& byUserId) {
                QString userName;
                if (clientId == session_.localClientId()) {
                    const CollabParticipant local = session_.localIdentity();
                    userName = local.userName;
                } else {
                    userName = session_.participant(clientId).userName;
                }
                session_.processLockGranted(layerId, clientId, byUserId, userName,
                                            QDateTime::currentMSecsSinceEpoch());
                notifyLockStateChanged(layerId);
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteLockReleased, receiver,
            [this](const QString& layerId, const QString&) {
                session_.processLockReleased(layerId);
                notifyLockStateChanged(layerId);
            }));

        connections_.append(QObject::connect(
            &ws_, &CollaborationWebSocket::remoteLockDenied, receiver,
            [this](const QString& layerId, const QString& reason) {
                session_.processLockDenied(layerId, reason);
                notifyLockStateChanged(layerId);
            }));
    }

    void notifyParticipantsChanged() {
        if (participantsChangedCallback_) participantsChangedCallback_();
    }

    void notifyLockStateChanged(const QString& layerId) {
        if (lockStateChangedCallback_) lockStateChangedCallback_(layerId);
    }

    CollaborationWebSocket& ws_;
    CollaborationSession& session_;
    QString projectId_;
    Array<QMetaObject::Connection> connections_;
    std::function<void()> participantsChangedCallback_;
    std::function<void(const QString&)> lockStateChangedCallback_;
    std::function<void(const CollabOperationData&)> operationAppliedCallback_;
    std::function<bool(const CollabOperationData&)> remoteOperationValidator_;
};

} // namespace ArtifactCore
