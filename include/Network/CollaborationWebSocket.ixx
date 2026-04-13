module;
#include <memory>
#include <QString>
#include <QJsonObject>
#include <QByteArray>

export module Network.CollaborationWebSocket;

export namespace ArtifactCore {

enum class CollabConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

struct CollabUserInfo {
    QString userId;
    QString userName;
    QString userColor;
};

class CollaborationWebSocket {
private:
    class Impl;
    Impl* impl_ = nullptr;

public:
    CollaborationWebSocket();
    ~CollaborationWebSocket();

    void connectToServer(const QString& serverUrl, const QString& projectId, const QString& clientId);
    void disconnect();
    bool isConnected() const;
    CollabConnectionState connectionState() const;

    QString serverUrl() const;
    QString projectId() const;
    QString clientId() const;

    // メッセージ送信
    void sendOperation(const QJsonObject& operation);
    void sendPresence(const QJsonObject& presence);
    void sendLockRequest(const QString& layerId);
    void sendUnlockRequest(const QString& layerId);
    void sendJoin(const QString& userId, const QString& userName, const QString& userColor);

    // シグナルハンドラ設定（Qtシグナルの代わりにstd::functionを使用）
    using OperationReceivedCallback = std::function<void(const QJsonObject&)>;
    using PresenceUpdatedCallback = std::function<void(const QJsonObject&)>;
    using LockGrantedCallback = std::function<void(const QString&)>;
    using LockDeniedCallback = std::function<void(const QString&, const QString&)>;
    using UserJoinedCallback = std::function<void(const CollabUserInfo&)>;
    using UserLeftCallback = std::function<void(const CollabUserInfo&)>;
    using ConnectionStateChangedCallback = std::function<void(CollabConnectionState)>;
    using HistoryReceivedCallback = std::function<void(const QJsonArray&)>;

    void setOperationReceivedCallback(OperationReceivedCallback cb);
    void setPresenceUpdatedCallback(PresenceUpdatedCallback cb);
    void setLockGrantedCallback(LockGrantedCallback cb);
    void setLockDeniedCallback(LockDeniedCallback cb);
    void setUserJoinedCallback(UserJoinedCallback cb);
    void setUserLeftCallback(UserLeftCallback cb);
    void setConnectionStateChangedCallback(ConnectionStateChangedCallback cb);
    void setHistoryReceivedCallback(HistoryReceivedCallback cb);

    // 内部処理用（QWebSocketのシグナルから呼び出す）
    void handleTextMessage(const QString& message);
    void handleConnected();
    void handleDisconnected();
    void handleError();
};

} // namespace ArtifactCore
