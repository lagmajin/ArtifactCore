module;
#include <QHash>
#include <QDateTime>
#include <QDebug>
#include <wobjectimpl.h>

module Command.LayerLockManager;

namespace ArtifactCore {

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
    Q_EMIT lockAcquired(layerId, clientId);
    Q_EMIT lockChanged();
    return true;
}

bool LayerLockManager::releaseLock(const QString& layerId, const QString& clientId)
{
    if (!locks_.contains(layerId)) return false;

    const LayerLockInfo& existing = locks_[layerId];
    if (existing.clientId != clientId) return false;

    locks_.remove(layerId);
    Q_EMIT lockReleased(layerId, clientId);
    Q_EMIT lockChanged();
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
        Q_EMIT lockDenied(layerId, reason);
        Q_EMIT lockChanged();
        qDebug() << "[LayerLock] Remote lock denied:" << layerId << reason;
    }
}

void LayerLockManager::onRemoteLockReleased(const QString& layerId, const QString& clientId)
{
    if (locks_.contains(layerId) && locks_[layerId].clientId == clientId) {
        locks_.remove(layerId);
        Q_EMIT lockReleased(layerId, clientId);
        Q_EMIT lockChanged();
    }
}

bool LayerLockManager::isLocked(const QString& layerId) const
{
    if (!locks_.contains(layerId)) return false;
    return !locks_[layerId].isExpired(lockTimeoutMs_);
}

const LayerLockInfo* LayerLockManager::lockInfo(const QString& layerId) const
{
    if (!locks_.contains(layerId)) return nullptr;
    if (locks_[layerId].isExpired(lockTimeoutMs_)) return nullptr;
    return &locks_[layerId];
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
        Q_EMIT lockReleased(id, locks_[id].clientId);
        locks_.remove(id);
    }
    if (!expired.isEmpty()) {
        Q_EMIT lockChanged();
    }
}

} // namespace ArtifactCore
