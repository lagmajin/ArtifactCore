module;
#include "../Define/DllExportMacro.hpp"
#include <QObject>
#include <QString>
#include <QHash>
#include <QDateTime>
#include <wobjectdefs.h>

export module Command.LayerLockManager;

export namespace ArtifactCore {

struct LayerLockInfo {
    QString layerId;
    QString clientId;
    QString userId;
    QString userName;
    qint64 acquiredAt = 0;

    bool isExpired(qint64 timeoutMs = 300000) const {
        return QDateTime::currentMSecsSinceEpoch() - acquiredAt > timeoutMs;
    }
};

enum class LayerLockChangeKind {
    Acquired,
    Released,
    Denied,
    Expired
};

struct LayerLockChangedEvent {
    LayerLockChangeKind kind = LayerLockChangeKind::Acquired;
    QString layerId;
    QString clientId;
    QString reason;
};

/**
 * @brief Manages collaborative layer locks.
 *
 * Tracks which client holds a lock on which layer.
 * Integrates with CollaborationWebSocket to send lock
 * requests and process remote lock state changes.
 */
class LIBRARY_DLL_API LayerLockManager : public QObject {
    W_OBJECT(LayerLockManager)

public:
    explicit LayerLockManager(QObject* parent = nullptr);
    ~LayerLockManager();

    /// Attempt to acquire a lock locally and send request to server.
    /// Returns true if the lock was acquired locally (server may deny later).
    bool acquireLock(const QString& layerId, const QString& clientId,
                     const QString& userId, const QString& userName);

    /// Release a lock locally and notify server.
    bool releaseLock(const QString& layerId, const QString& clientId);

    /// Handle a lock_granted message from the server.
    void onRemoteLockGranted(const QString& layerId, const QString& userId);

    /// Handle a lock_denied message from the server.
    void onRemoteLockDenied(const QString& layerId, const QString& reason);

    /// Handle a lock_released message from the server.
    void onRemoteLockReleased(const QString& layerId, const QString& clientId);

    /// Check if a layer is locked (local or remote).
    bool isLocked(const QString& layerId) const;

    /// Get lock info for a layer, or nullptr if not locked.
    const LayerLockInfo* lockInfo(const QString& layerId) const;

    /// Get all active locks.
    QList<LayerLockInfo> activeLocks() const;

    /// Remove expired locks. Call periodically.
    void purgeExpired(qint64 timeoutMs = 300000);

    /// Set lock timeout in ms (default 5 minutes).
    void setLockTimeout(qint64 timeoutMs) { lockTimeoutMs_ = timeoutMs; }

private:
    QHash<QString, LayerLockInfo> locks_;
    qint64 lockTimeoutMs_ = 300000;
};

} // namespace ArtifactCore
