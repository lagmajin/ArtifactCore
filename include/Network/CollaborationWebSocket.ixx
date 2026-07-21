module;
#include "../Define/DllExportMacro.hpp"
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <wobjectdefs.h>

export module Network.CollaborationWebSocket;

export namespace ArtifactCore {

// --- Connection state ---
enum class CollabConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

// --- Message structs ---

struct JoinMessage {
    QString projectId;
    QString clientId;
    QString userId;
    QString userName;
    QString userColor;

    QJsonObject toJson() const;
};

struct OperationMessage {
    QString clientId;
    QString projectId;
    QJsonObject operation;
    int version = 0;
    QString timestamp;

    QJsonObject toJson() const;
};

struct LockRequestMessage {
    QString layerId;
    QString clientId;

    QJsonObject toJson() const;
};

struct LockReleaseMessage {
    QString layerId;
    QString clientId;

    QJsonObject toJson() const;
};

struct PresenceMessage {
    QString clientId;
    QString userId;
    QString userName;
    QString userColor;
    QJsonObject presence;

    QJsonObject toJson() const;
};

/**
 * @brief Qt WebSocket client for the collaboration server.
 *
 * Connects to tools/collaboration-server, handles the 5-message protocol
 * (join/operation/lock_request/unlock_request/presence), automatic
 * reconnection with exponential back-off, and heartbeat.
 */
class LIBRARY_DLL_API CollaborationWebSocket : public QObject {
    W_OBJECT(CollaborationWebSocket)

public:
    explicit CollaborationWebSocket(QObject* parent = nullptr);
    ~CollaborationWebSocket();

    void connectToServer(const QString& serverUrl, const JoinMessage& join);
    void disconnect();

    bool isConnected() const;
    CollabConnectionState connectionState() const;

    // --- Outgoing ---
    void sendOperation(const OperationMessage& op);
    void sendLockRequest(const LockRequestMessage& req);
    void sendLockRelease(const LockReleaseMessage& rel);
    void sendPresence(const PresenceMessage& pres);

public:
    // --- Incoming signals ---
    void remoteOperation(const OperationMessage& op)
        W_SIGNAL(remoteOperation, op);
    void remoteLockGranted(const QString& layerId, const QString& byUserId)
        W_SIGNAL(remoteLockGranted, layerId, byUserId);
    void remoteLockDenied(const QString& layerId, const QString& reason)
        W_SIGNAL(remoteLockDenied, layerId, reason);
    void remoteLockReleased(const QString& layerId, const QString& byUserId)
        W_SIGNAL(remoteLockReleased, layerId, byUserId);
    void remotePresence(const PresenceMessage& pres)
        W_SIGNAL(remotePresence, pres);
    void userJoined(const QString& userId, const QString& userName)
        W_SIGNAL(userJoined, userId, userName);
    void userLeft(const QString& userId, const QString& userName)
        W_SIGNAL(userLeft, userId, userName);
    void connectionStateChanged(CollabConnectionState state)
        W_SIGNAL(connectionStateChanged, state);
    void protocolError(const QString& message)
        W_SIGNAL(protocolError, message);

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
