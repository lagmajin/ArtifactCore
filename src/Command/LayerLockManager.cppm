module;
#include <utility>
#include <QHash>
#include <QSet>
#include <chrono>
#include <QDebug>

module Command.LayerLockManager;

import std;

namespace ArtifactCore {

class LayerLockManager::Impl {
public:
    QHash<QString, LayerLockInfo> activeLocks_;  // layerId -> LockInfo
    QHash<QString, ClientInfo> clients_;         // clientId -> ClientInfo
    std::chrono::seconds defaultTimeout_{300};   // デフォルト5分

    bool isExpired(const LayerLockInfo& info) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info.acquiredAt);
        return elapsed >= info.timeout;
    }
};

LayerLockManager::LayerLockManager(QObject* parent)
    : QObject(parent)
    , impl_(new Impl())
{
}

LayerLockManager::~LayerLockManager() {
    delete impl_;
}

bool LayerLockManager::requestLock(const QString& layerId, const QString& clientId, const QString& userId, const QString& userName) {
    // クライアント登録確認
    if (!impl_->clients_.contains(clientId)) {
        emit lockRequestDenied(layerId, clientId, QStringLiteral("Client not registered"));
        return false;
    }

    // 既にロック済み
    if (hasLock(layerId, clientId)) {
        return true;
    }

    // 他ユーザーがロック中
    if (isLayerLocked(layerId)) {
        const auto& info = impl_->activeLocks_[layerId];
        QString reason = QStringLiteral("Locked by %1 (%2)").arg(info.userName, info.clientId);
        emit lockRequestDenied(layerId, clientId, reason);
        return false;
    }

    // タイムアウトチェック
    checkTimeouts();

    // ロック付与
    LayerLockInfo lockInfo;
    lockInfo.layerId = layerId;
    lockInfo.clientId = clientId;
    lockInfo.userId = userId;
    lockInfo.userName = userName;
    lockInfo.acquiredAt = std::chrono::steady_clock::now();
    lockInfo.timeout = impl_->defaultTimeout_;

    impl_->activeLocks_[layerId] = lockInfo;

    emit lockAcquired(layerId, clientId);
    return true;
}

void LayerLockManager::releaseLock(const QString& layerId, const QString& clientId) {
    auto it = impl_->activeLocks_.find(layerId);
    if (it != impl_->activeLocks_.end() && it->clientId == clientId) {
        impl_->activeLocks_.erase(it);
        emit lockReleased(layerId, clientId);
    }
}

void LayerLockManager::forceReleaseLock(const QString& layerId) {
    auto it = impl_->activeLocks_.find(layerId);
    if (it != impl_->activeLocks_.end()) {
        QString clientId = it->clientId;
        impl_->activeLocks_.erase(it);
        emit lockReleased(layerId, clientId);
    }
}

bool LayerLockManager::hasLock(const QString& layerId, const QString& clientId) const {
    auto it = impl_->activeLocks_.find(layerId);
    return it != impl_->activeLocks_.end() && it->clientId == clientId;
}

bool LayerLockManager::isLayerLocked(const QString& layerId) const {
    auto it = impl_->activeLocks_.find(layerId);
    if (it == impl_->activeLocks_.end()) return false;

    // タイムアウトチェック
    if (impl_->isExpired(*it)) {
        // 期限切れだが、明示的な解放は行わない（checkTimeouts()呼び出し時に一括処理）
        return false;
    }
    return true;
}

LayerLockInfo LayerLockManager::getLockInfo(const QString& layerId) const {
    auto it = impl_->activeLocks_.find(layerId);
    if (it != impl_->activeLocks_.end()) {
        return *it;
    }
    return LayerLockInfo{};
}

QSet<QString> LayerLockManager::getLockedLayers() const {
    QSet<QString> locked;
    for (auto it = impl_->activeLocks_.begin(); it != impl_->activeLocks_.end(); ++it) {
        if (!impl_->isExpired(it.value())) {
            locked.insert(it.key());
        }
    }
    return locked;
}

QSet<QString> LayerLockManager::getLockedLayersByClient(const QString& clientId) const {
    QSet<QString> locked;
    for (auto it = impl_->activeLocks_.begin(); it != impl_->activeLocks_.end(); ++it) {
        if (it->clientId == clientId && !impl_->isExpired(it.value())) {
            locked.insert(it.key());
        }
    }
    return locked;
}

void LayerLockManager::checkTimeouts() {
    QStringList expiredLayers;
    for (auto it = impl_->activeLocks_.begin(); it != impl_->activeLocks_.end(); ++it) {
        if (impl_->isExpired(it.value())) {
            expiredLayers.append(it.key());
        }
    }

    for (const QString& layerId : expiredLayers) {
        auto it = impl_->activeLocks_.find(layerId);
        if (it != impl_->activeLocks_.end()) {
            QString clientId = it->clientId;
            qWarning() << "[LayerLockManager] Lock timed out for layer:" << layerId << "client:" << clientId;
            impl_->activeLocks_.erase(it);
            emit lockTimedOut(layerId, clientId);
        }
    }
}

void LayerLockManager::setDefaultTimeout(std::chrono::seconds timeout) {
    impl_->defaultTimeout_ = timeout;
}

void LayerLockManager::releaseAllLocks(const QString& clientId) {
    QStringList layersToRemove;
    for (auto it = impl_->activeLocks_.begin(); it != impl_->activeLocks_.end(); ++it) {
        if (it->clientId == clientId) {
            layersToRemove.append(it.key());
        }
    }

    for (const QString& layerId : layersToRemove) {
        impl_->activeLocks_.remove(layerId);
        emit lockReleased(layerId, clientId);
    }
}

QHash<QString, LayerLockManager::ClientInfo> LayerLockManager::registeredClients() const {
    return impl_->clients_;
}

void LayerLockManager::registerClient(const QString& clientId, const QString& userId, const QString& userName) {
    ClientInfo info;
    info.clientId = clientId;
    info.userId = userId;
    info.userName = userName;
    impl_->clients_[clientId] = info;
}

void LayerLockManager::unregisterClient(const QString& clientId) {
    // クライアントのロックを全解放
    releaseAllLocks(clientId);
    impl_->clients_.remove(clientId);
}

} // namespace ArtifactCore
