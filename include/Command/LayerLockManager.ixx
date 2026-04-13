module;
#include <memory>
#include <QString>
#include <QHash>
#include <QSet>
#include <chrono>
#include <QObject>
#include <wobjectdefs.h>

export module Command.LayerLockManager;

export namespace ArtifactCore {

struct LayerLockInfo {
    QString layerId;
    QString clientId;
    QString userId;
    QString userName;
    std::chrono::steady_clock::time_point acquiredAt;
    std::chrono::seconds timeout;
};

class LayerLockManager : public QObject {
    W_OBJECT(LayerLockManager)
public:
    LayerLockManager(QObject* parent = nullptr);
    ~LayerLockManager();

    // ロック操作
    bool requestLock(const QString& layerId, const QString& clientId, const QString& userId, const QString& userName);
    void releaseLock(const QString& layerId, const QString& clientId);
    void forceReleaseLock(const QString& layerId);
    bool hasLock(const QString& layerId, const QString& clientId) const;
    bool isLayerLocked(const QString& layerId) const;
    LayerLockInfo getLockInfo(const QString& layerId) const;
    QSet<QString> getLockedLayers() const;
    QSet<QString> getLockedLayersByClient(const QString& clientId) const;

    // タイムアウトしたロックを自動解放
    void checkTimeouts();
    void setDefaultTimeout(std::chrono::seconds timeout);

    // 全ロック解放（切断時等）
    void releaseAllLocks(const QString& clientId);

    // ユーザー情報
    struct ClientInfo {
        QString clientId;
        QString userId;
        QString userName;
    };
    QHash<QString, ClientInfo> registeredClients() const;
    void registerClient(const QString& clientId, const QString& userId, const QString& userName);
    void unregisterClient(const QString& clientId);

signals:
    void lockAcquired(const QString& layerId, const QString& clientId)
        W_SIGNAL(lockAcquired, layerId, clientId);
    void lockReleased(const QString& layerId, const QString& clientId)
        W_SIGNAL(lockReleased, layerId, clientId);
    void lockRequestDenied(const QString& layerId, const QString& clientId, const QString& reason)
        W_SIGNAL(lockRequestDenied, layerId, clientId, reason);
    void lockTimedOut(const QString& layerId, const QString& clientId)
        W_SIGNAL(lockTimedOut, layerId, clientId);

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace ArtifactCore
