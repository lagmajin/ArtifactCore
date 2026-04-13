module;
#include <utility>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDebug>

module Command.CollaborationManager;

import std;
import Network.CollaborationWebSocket;
import Command.Session;
import Command.OperationTransformer;

namespace ArtifactCore {

class CollaborationManager::Impl {
public:
    std::shared_ptr<CollaborationWebSocket> wsClient_;
    EditSession* editSession_ = nullptr;

    QString userId_;
    QString userName_;
    QString userColor_;
    QString clientId_;
    QString projectId_;

    QList<UserInfo> connectedUsers_;
    QSet<QString> lockedLayers_;

    // Callbacks
    RemoteOperationCallback onRemoteOperation_;
    UserJoinedCallback onUserJoined_;
    UserLeftCallback onUserLeft_;
    LockStateChangedCallback onLockStateChanged_;

    Impl() {
        clientId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    ~Impl() = default;

    void setupCallbacks() {
        if (!wsClient_) return;

        wsClient_->setOperationReceivedCallback([this](const QJsonObject& msg) {
            QJsonObject operation = msg.value(QStringLiteral("operation")).toObject();

            // OT変換を適用
            // 未実装: 本来は履歴と突き合わせて変換する
            QJsonObject transformed = OperationTransformer::transform(operation, {});

            // ローカルのEditSessionに適用
            if (editSession_) {
                editSession_->applyRemoteOperation(transformed);
            }

            if (onRemoteOperation_) {
                onRemoteOperation_(transformed);
            }
        });

        wsClient_->setUserJoinedCallback([this](const CollabUserInfo& user) {
            UserInfo info;
            info.userId = user.userId;
            info.userName = user.userName;
            info.userColor = user.userColor;
            connectedUsers_.append(info);

            if (onUserJoined_) {
                onUserJoined_(info);
            }
        });

        wsClient_->setUserLeftCallback([this](const CollabUserInfo& user) {
            UserInfo info;
            info.userId = user.userId;
            info.userName = user.userName;
            info.userColor = user.userColor;

            // リストから削除
            for (int i = 0; i < connectedUsers_.size(); ++i) {
                if (connectedUsers_[i].userId == user.userId) {
                    connectedUsers_.removeAt(i);
                    break;
                }
            }

            // ユーザーが持っていたロックを解放
            for (auto it = lockedLayers_.begin(); it != lockedLayers_.end(); ) {
                // 簡易実装: 全ロックを再チェック
                ++it;
            }

            if (onUserLeft_) {
                onUserLeft_(info);
            }
        });

        wsClient_->setLockGrantedCallback([this](const QString& layerId) {
            lockedLayers_.insert(layerId);
            if (onLockStateChanged_) {
                onLockStateChanged_(layerId, true);
            }
        });

        wsClient_->setLockDeniedCallback([this](const QString& layerId, const QString& reason) {
            qWarning() << "[CollabManager] Lock denied for" << layerId << ":" << reason;
            if (onLockStateChanged_) {
                onLockStateChanged_(layerId, false);
            }
        });

        wsClient_->setHistoryReceivedCallback([this](const QJsonArray& operations) {
            if (!editSession_) return;

            // 履歴をローカルに適用
            for (const auto& opVal : operations) {
                QJsonObject op = opVal.toObject();
                editSession_->applyRemoteOperation(op);
            }

            qDebug() << "[CollabManager] Applied" << operations.size() << "historical operations";
        });
    }
};

CollaborationManager::CollaborationManager() : impl_(new Impl()) {}
CollaborationManager::~CollaborationManager() { delete impl_; }

void CollaborationManager::connectToServer(const QString& serverUrl, const QString& projectId) {
    impl_->projectId_ = projectId;
    impl_->wsClient_ = std::make_shared<CollaborationWebSocket>();
    impl_->setupCallbacks();

    impl_->wsClient_->connectToServer(serverUrl, projectId, impl_->clientId_);
    impl_->wsClient_->sendJoin(impl_->userId_, impl_->userName_, impl_->userColor_);

    // EditSessionにコラボレーションクライアントを設定
    if (impl_->editSession_) {
        impl_->editSession_->setCollaborationClient(impl_->wsClient_);
        impl_->editSession_->setCollaborationEnabled(true);
    }
}

void CollaborationManager::disconnect() {
    if (impl_->wsClient_) {
        impl_->wsClient_->disconnect();
    }
    impl_->connectedUsers_.clear();
    impl_->lockedLayers_.clear();
}

bool CollaborationManager::isConnected() const {
    return impl_->wsClient_ && impl_->wsClient_->isConnected();
}

void CollaborationManager::setUserInfo(const QString& userId, const QString& userName, const QString& userColor) {
    impl_->userId_ = userId;
    impl_->userName_ = userName;
    impl_->userColor_ = userColor;
}

void CollaborationManager::setEditSession(EditSession* session) {
    impl_->editSession_ = session;
    if (impl_->wsClient_ && isConnected()) {
        session->setCollaborationClient(impl_->wsClient_);
        session->setCollaborationEnabled(true);
    }
}

EditSession* CollaborationManager::editSession() const {
    return impl_->editSession_;
}

void CollaborationManager::sendOperation(const QJsonObject& operation) {
    if (impl_->wsClient_ && isConnected()) {
        impl_->wsClient_->sendOperation(operation);
    }
}

void CollaborationManager::applyRemoteOperation(const QJsonObject& operation) {
    if (impl_->editSession_) {
        impl_->editSession_->applyRemoteOperation(operation);
    }
}

void CollaborationManager::updatePresence(const QJsonObject& presence) {
    if (impl_->wsClient_ && isConnected()) {
        impl_->wsClient_->sendPresence(presence);
    }
}

void CollaborationManager::requestLayerLock(const QString& layerId) {
    if (impl_->wsClient_ && isConnected()) {
        impl_->wsClient_->sendLockRequest(layerId);
    }
}

void CollaborationManager::releaseLayerLock(const QString& layerId) {
    if (impl_->wsClient_ && isConnected()) {
        impl_->wsClient_->sendUnlockRequest(layerId);
        impl_->lockedLayers_.remove(layerId);
    }
}

bool CollaborationManager::hasLayerLock(const QString& layerId) const {
    return impl_->lockedLayers_.contains(layerId);
}

QList<CollaborationManager::UserInfo> CollaborationManager::connectedUsers() const {
    return impl_->connectedUsers_;
}

CollaborationManager::UserInfo CollaborationManager::localUser() const {
    UserInfo info;
    info.userId = impl_->userId_;
    info.userName = impl_->userName_;
    info.userColor = impl_->userColor_;
    return info;
}

void CollaborationManager::setRemoteOperationCallback(RemoteOperationCallback cb) {
    impl_->onRemoteOperation_ = std::move(cb);
}

void CollaborationManager::setUserJoinedCallback(UserJoinedCallback cb) {
    impl_->onUserJoined_ = std::move(cb);
}

void CollaborationManager::setUserLeftCallback(UserLeftCallback cb) {
    impl_->onUserLeft_ = std::move(cb);
}

void CollaborationManager::setLockStateChangedCallback(LockStateChangedCallback cb) {
    impl_->onLockStateChanged_ = std::move(cb);
}

} // namespace ArtifactCore
