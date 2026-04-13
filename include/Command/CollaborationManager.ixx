module;
#include <memory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

export module Command.CollaborationManager;

import Network.CollaborationWebSocket;
import Command.Session;
import Command.OperationTransformer;

export namespace ArtifactCore {

/**
 * @brief コラボレーションセッションの管理
 *
 * EditSessionとCollaborationWebSocketを橋渡しし、
 * リアルタイム同期のオーケストレーションを行う。
 */
class CollaborationManager {
private:
    class Impl;
    Impl* impl_ = nullptr;

public:
    CollaborationManager();
    ~CollaborationManager();

    // 接続管理
    void connectToServer(const QString& serverUrl, const QString& projectId);
    void disconnect();
    bool isConnected() const;

    // ユーザー情報
    void setUserInfo(const QString& userId, const QString& userName, const QString& userColor);

    // EditSessionとの統合
    void setEditSession(EditSession* session);
    EditSession* editSession() const;

    // 操作の送信
    void sendOperation(const QJsonObject& operation);

    // リモート操作の適用
    void applyRemoteOperation(const QJsonObject& operation);

    // プレゼンス
    void updatePresence(const QJsonObject& presence);

    // ロック
    void requestLayerLock(const QString& layerId);
    void releaseLayerLock(const QString& layerId);
    bool hasLayerLock(const QString& layerId) const;

    // ユーザー情報
    struct UserInfo {
        QString userId;
        QString userName;
        QString userColor;
    };

    QList<UserInfo> connectedUsers() const;
    UserInfo localUser() const;

    // コールバック
    using RemoteOperationCallback = std::function<void(const QJsonObject&)>;
    using UserJoinedCallback = std::function<void(const UserInfo&)>;
    using UserLeftCallback = std::function<void(const UserInfo&)>;
    using LockStateChangedCallback = std::function<void(const QString&, bool)>;

    void setRemoteOperationCallback(RemoteOperationCallback cb);
    void setUserJoinedCallback(UserJoinedCallback cb);
    void setUserLeftCallback(UserLeftCallback cb);
    void setLockStateChangedCallback(LockStateChangedCallback cb);
};

} // namespace ArtifactCore
