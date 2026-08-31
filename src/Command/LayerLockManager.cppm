module;
#include <QHash>
#include <QDateTime>
#include <QDebug>
#include <wobjectimpl.h>

module Command.LayerLockManager;

import Event.Bus;

namespace ArtifactCore {

namespace {
void publishLayerLockChanged(LayerLockChangeKind kind,
                             const QString& layerId = {},
                             const QString& clientId = {},
                             const QString& reason = {}) {
    globalEventBus().publish(LayerLockChangedEvent{kind, layerId, clientId, reason});
}
}

W_OBJECT_IMPL(LayerLockManager)

LayerLockManager::LayerLockManager(QObject* parent) : QObject(parent) {}

LayerLockManager::~LayerLockManager() = default;

bool LayerLockManager::acquireLock(const QString& layerId, const QString& clientId,
                                    const QString& userId, const QString& userName)
{
    // Check if already locked by someone else
    if (locks_.contains(layerId)) {
        const LayerLockInfo& existing = locks_[layerId];
        if (!existing.isExpired(lockTimeoutMs_) && existing.clientId != clientId) {
            return false;
        }
        // Expired or same client - remove old lock
        locks_.remove(layerId);
    }

    LayerLockInfo info;
    info.layerId = layerId;
    info.clientId = clientId;
    info.userId = userId;
    info.userName = userName;
    info.acquiredAt = QDateTime::currentMSecsSinceEpoch();

    locks_[layerId] = info;
    publishLayerLockChanged(LayerLockChangeKind::Acquired, layerId, clientId);
    return true;
}

bool LayerLockManager::releaseLock(const QString& layerId, const QString& clientId)
{
    if (!locks_.contains(layerId)) return false;

    const LayerLockInfo& existing = locks_[layerId];
    if (existing.clientId != clientId) return false;

    locks_.remove(layerId);
    publishLayerLockChanged(LayerLockChangeKind::Released, layerId, clientId);
    return true;
}

void LayerLockManager::onRemoteLockGranted(const QString& layerId, const QString& userId)
{
    // Server confirmed our lock - nothing to do locally (already optimistic)
    qDebug() << "[LayerLock] Remote lock granted:" << layerId << "by" << userId;
}

void LayerLockManager::onRemoteLockDenied(const QString& layerId, const QString& reason)
{
    // Server denied our lock request - release local optimistic lock
    if (locks_.contains(layerId)) {
        QString clientId = locks_[layerId].clientId;
        locks_.remove(layerId);
        publishLayerLockChanged(LayerLockChangeKind::Denied, layerId, clientId, reason);
        qDebug() << "[LayerLock] Remote lock denied:" << layerId << reason;
    }
}

void LayerLockManager::onRemoteLockReleased(const QString& layerId, const QString& clientId)
{
    if (locks_.contains(layerId) && locks_[layerId].clientId == clientId) {
        locks_.remove(layerId);
        publishLayerLockChanged(LayerLockChangeKind::Released, layerId, clientId);
    }
}

bool LayerLockManager::isLocked(const QString& layerId) const
{
    if (!locks_.contains(layerId)) return false;
    return !locks_[layerId].isExpired(lockTimeoutMs_);
}

const LayerLockInfo* LayerLockManager::lockInfo(const QString& layerId) const
{
    const auto it = locks_.constFind(layerId);
    if (it == locks_.constEnd()) return nullptr;
    if (it.value().isExpired(lockTimeoutMs_)) return nullptr;
    return &it.value();
}

QList<LayerLockInfo> LayerLockManager::activeLocks() const
{
    QList<LayerLockInfo> result;
    for (auto it = locks_.begin(); it != locks_.end(); ++it) {
        if (!it.value().isExpired(lockTimeoutMs_)) {
            result.append(it.value());
        }
    }
    return result;
}

void LayerLockManager::purgeExpired(qint64 timeoutMs)
{
    QList<QString> expired;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = locks_.begin(); it != locks_.end(); ++it) {
        if (now - it.value().acquiredAt > timeoutMs) {
            expired.append(it.key());
        }
    }
    for (const QString& id : expired) {
        publishLayerLockChanged(LayerLockChangeKind::Expired, id, locks_[id].clientId);
        locks_.remove(id);
    }
}

} // namespace ArtifactCore
