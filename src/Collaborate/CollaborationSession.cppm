module;

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QHash>
#include <QDateTime>

export module Collaborate.Session;

// Re-exported so consumers of session APIs can use the container types.
export import Core.ArtifactArray;

export namespace ArtifactCore {

// Typed ephemeral presence payload (safe to drop at any time). Unknown keys
// survive round-trips through `raw`.
struct CollabPresenceState {
    bool hasCursor = false;
    double cursorX = 0.0;
    double cursorY = 0.0;
    bool hasSelection = false;
    QString selectedLayerId;
    bool hasComposition = false;
    QString compositionId;
    bool hasPlayback = false;
    qint64 playbackFrame = -1;
    QString statusText;
    QJsonObject raw;

    [[nodiscard]] static CollabPresenceState fromJson(const QJsonObject& obj) {
        CollabPresenceState state;
        state.raw = obj;
        const auto cursor = obj.value(QStringLiteral("cursor")).toObject();
        if (cursor.contains(QStringLiteral("x")) &&
            cursor.contains(QStringLiteral("y"))) {
            state.hasCursor = true;
            state.cursorX = cursor.value(QStringLiteral("x")).toDouble();
            state.cursorY = cursor.value(QStringLiteral("y")).toDouble();
        }
        const QString selected =
            obj.value(QStringLiteral("selectedLayerId")).toString();
        if (!selected.isEmpty()) {
            state.hasSelection = true;
            state.selectedLayerId = selected;
        }
        const QString composition =
            obj.value(QStringLiteral("compositionId")).toString();
        if (!composition.isEmpty()) {
            state.hasComposition = true;
            state.compositionId = composition;
        }
        if (obj.contains(QStringLiteral("playbackFrame"))) {
            state.hasPlayback = true;
            state.playbackFrame =
                obj.value(QStringLiteral("playbackFrame")).toVariant().toLongLong();
        }
        state.statusText = obj.value(QStringLiteral("status")).toString();
        return state;
    }

    [[nodiscard]] QJsonObject toJson() const {
        if (!raw.isEmpty()) {
            QJsonObject merged = raw;
            if (hasCursor) {
                merged.insert(QStringLiteral("cursor"),
                              QJsonObject{{QStringLiteral("x"), cursorX},
                                          {QStringLiteral("y"), cursorY}});
            }
            if (hasSelection) {
                merged.insert(QStringLiteral("selectedLayerId"), selectedLayerId);
            }
            if (hasComposition) {
                merged.insert(QStringLiteral("compositionId"), compositionId);
            }
            if (hasPlayback) {
                merged.insert(QStringLiteral("playbackFrame"),
                              static_cast<qint64>(playbackFrame));
            }
            if (!statusText.isEmpty()) {
                merged.insert(QStringLiteral("status"), statusText);
            }
            return merged;
        }
        QJsonObject obj;
        if (hasCursor) {
            obj.insert(QStringLiteral("cursor"),
                       QJsonObject{{QStringLiteral("x"), cursorX},
                                   {QStringLiteral("y"), cursorY}});
        }
        if (hasSelection) {
            obj.insert(QStringLiteral("selectedLayerId"), selectedLayerId);
        }
        if (hasComposition) {
            obj.insert(QStringLiteral("compositionId"), compositionId);
        }
        if (hasPlayback) {
            obj.insert(QStringLiteral("playbackFrame"),
                       static_cast<qint64>(playbackFrame));
        }
        if (!statusText.isEmpty()) {
            obj.insert(QStringLiteral("status"), statusText);
        }
        return obj;
    }
};

// Transport-independent collaboration session model. The WebSocket client
// (Network.CollaborationWebSocket) feeds inbound messages into process*
// methods and sends the structs returned by create* methods; this class owns
// roster, lock ledger, and operation-log state only. No QObject, no I/O.

struct CollabParticipant {
    QString clientId;
    QString userId;
    QString userName;
    QString userColor;
    qint64 joinedAtMs = 0;
    qint64 lastPresenceMs = 0;
    QJsonObject presence;
};

struct CollabLayerLock {
    QString layerId;
    QString clientId;
    QString userId;
    QString userName;
    qint64 acquiredAtMs = 0;
};

struct CollabOperationData {
    QString type;
    QString layerId;
    QJsonObject payload;
    qint64 version = -1;   // server-assigned; -1 while pending echo
    qint64 sequence = -1;  // local unique id; survives server relay (unknown
                           // fields are preserved through the spread) and
                           // disambiguates same-millisecond operations
    QString clientId;
    qint64 timestampMs = 0;

    [[nodiscard]] QString dedupeKey() const {
        if (sequence >= 0) {
            return QStringLiteral("seq:%1|%2").arg(clientId, QString::number(sequence));
        }
        return QStringLiteral("%1|%2|%3|%4")
            .arg(clientId, type, layerId, QString::number(timestampMs));
    }

    [[nodiscard]] QJsonObject toJson() const {
        QJsonObject obj;
        obj[QStringLiteral("type")] = type;
        obj[QStringLiteral("layerId")] = layerId;
        obj[QStringLiteral("payload")] = payload;
        if (version >= 0) {
            obj[QStringLiteral("version")] = static_cast<qint64>(version);
        }
        if (sequence >= 0) {
            // Unknown fields survive the server relay verbatim, so this is
            // the reliable echo-correlation token.
            obj[QStringLiteral("opSeq")] = static_cast<qint64>(sequence);
        }
        obj[QStringLiteral("clientId")] = clientId;
        obj[QStringLiteral("timestamp")] = static_cast<qint64>(timestampMs);
        return obj;
    }

    [[nodiscard]] static CollabOperationData fromJson(const QJsonObject& obj) {
        CollabOperationData op;
        op.type = obj.value(QStringLiteral("type")).toString();
        op.layerId = obj.value(QStringLiteral("layerId")).toString();
        op.payload = obj.value(QStringLiteral("payload")).toObject();
        op.version = obj.contains(QStringLiteral("version"))
                         ? obj.value(QStringLiteral("version")).toVariant().toLongLong()
                         : -1;
        op.sequence = obj.contains(QStringLiteral("opSeq"))
                          ? obj.value(QStringLiteral("opSeq")).toVariant().toLongLong()
                          : -1;
        op.clientId = obj.value(QStringLiteral("clientId")).toString();
        op.timestampMs =
            obj.value(QStringLiteral("timestamp")).toVariant().toLongLong();
        return op;
    }
};

class CollaborationSession {
public:
    void setLocalIdentity(const QString& clientId, const QString& userId,
                          const QString& userName, const QString& userColor) {
        local_ = CollabParticipant{clientId, userId, userName, userColor, 0, 0, {}};
    }

    [[nodiscard]] QString localClientId() const { return local_.clientId; }
    [[nodiscard]] CollabParticipant localIdentity() const { return local_; }

    // ---- roster ----

    void processUserJoined(const QString& clientId, const QString& userId,
                           const QString& userName, const QString& userColor,
                           const qint64 atMs) {
        if (clientId == local_.clientId) return;
        auto& participant = participants_[clientId];
        participant.clientId = clientId;
        participant.userId = userId;
        participant.userName = userName;
        participant.userColor = userColor;
        participant.joinedAtMs = atMs;
        participant.lastPresenceMs = atMs;
    }

    void processUserLeft(const QString& clientId) {
        participants_.remove(clientId);
        // Server releases the departed peer's locks; mirror that locally.
        for (auto it = locks_.begin(); it != locks_.end();) {
            if (it->clientId == clientId) it = locks_.erase(it);
            else ++it;
        }
    }

    void processPresence(const QString& clientId, const QJsonObject& presence,
                         const qint64 atMs) {
        const auto it = participants_.constFind(clientId);
        if (it == participants_.constEnd()) return;
        it->presence = presence;
        it->lastPresenceMs = atMs;
    }

    // Typed variant: parses the standard presence keys and keeps unknown
    // fields in `raw`.
    void processPresence(const QString& clientId,
                         const CollabPresenceState& state, const qint64 atMs) {
        processPresence(clientId, state.toJson(), atMs);
    }

    [[nodiscard]] CollabPresenceState participantPresence(
        const QString& clientId) const {
        return CollabPresenceState::fromJson(
            participants_.value(clientId).presence);
    }

    // Heartbeat-timeout detection: clients whose last presence (or join) is
    // older than `timeoutMs` relative to `nowMs`. Local identity is never
    // reported stale.
    [[nodiscard]] Array<QString> staleParticipantClientIds(
        const qint64 nowMs, const qint64 timeoutMs) const {
        Array<QString> stale;
        if (timeoutMs <= 0) return stale;
        for (auto it = participants_.constBegin();
             it != participants_.constEnd(); ++it) {
            const qint64 lastSeen =
                it->lastPresenceMs > 0 ? it->lastPresenceMs : it->joinedAtMs;
            if (lastSeen > 0 && nowMs - lastSeen > timeoutMs) {
                stale.append(it.key());
            }
        }
        return stale;
    }

    [[nodiscard]] CollabParticipant participant(const QString& clientId) const {
        return participants_.value(clientId);
    }
    [[nodiscard]] bool hasParticipant(const QString& clientId) const {
        return participants_.contains(clientId);
    }
    [[nodiscard]] Array<CollabParticipant> participants() const {
        Array<CollabParticipant> result;
        for (auto it = participants_.constBegin();
             it != participants_.constEnd(); ++it) {
            result.append(it.value());
        }
        return result;
    }

    // ---- operations ----

    // Builds a local operation (client id + timestamp stamped, version
    // pending). Caller sends it through the transport; the server echo is
    // reconciled by processRemoteOperation().
    [[nodiscard]] CollabOperationData createLocalOperation(
        const QString& type, const QString& layerId, const QJsonObject& payload,
        const qint64 atMs) {
        CollabOperationData request;
        request.type = type;
        request.layerId = layerId;
        request.payload = payload;
        request.timestampMs = atMs;
        return createLocalOperation(request, atMs);
    }

    // Typed-request overload: identity, sequence, and pending version are
    // always stamped by the session regardless of what the caller preset.
    [[nodiscard]] CollabOperationData createLocalOperation(
        const CollabOperationData& request, const qint64 atMs) {
        CollabOperationData op = request;
        op.version = -1;
        op.sequence = ++localOperationSequence_;
        op.clientId = local_.clientId;
        if (op.timestampMs <= 0) {
            op.timestampMs = atMs;
        }
        seenKeys_.insert(op.dedupeKey());
        operationLog_.append(op);
        return op;
    }

    void processRemoteOperation(const CollabOperationData& operation) {
        const QString key = operation.dedupeKey();
        if (seenKeys_.contains(key)) {
            // Echo of a local operation: record only the server-assigned
            // version. The relay overwrites the top-level timestamp, so the
            // log keeps our original creation time.
            if (operation.version >= 0) {
                for (auto& logged : operationLog_) {
                    if (logged.dedupeKey() == key && logged.version < 0) {
                        logged.version = operation.version;
                        break;
                    }
                }
            }
            return;
        }
        seenKeys_.insert(key);
        CollabOperationData op = operation;
        if (op.version < 0) {
            op.version = nextRemoteVersion();
        }
        lastRemoteVersion_ = op.version > lastRemoteVersion_ ? op.version : lastRemoteVersion_;
        operationLog_.append(op);
    }

    void processHistory(const Array<CollabOperationData>& operations) {
        for (const auto& operation : operations) {
            processRemoteOperation(operation);
        }
    }

    // Full log in arrival order (local pending ops carry version -1).
    [[nodiscard]] const Array<CollabOperationData>& operationLog() const {
        return operationLog_;
    }

    [[nodiscard]] qint64 latestVersion() const { return lastRemoteVersion_; }

    [[nodiscard]] int pendingLocalOperationCount() const {
        int pending = 0;
        for (const auto& logged : operationLog_) {
            if (logged.clientId == local_.clientId && logged.version < 0) {
                ++pending;
            }
        }
        return pending;
    }

    // ---- layer locks ----

    void processLockGranted(const QString& layerId, const QString& clientId,
                            const QString& userId, const QString& userName,
                            const qint64 atMs) {
        CollabLayerLock lock;
        lock.layerId = layerId;
        lock.clientId = clientId;
        lock.userId = userId;
        lock.userName = userName;
        lock.acquiredAtMs = atMs;
        locks_.insert(layerId, lock);
        pendingLockRequests_.remove(layerId);
        lockDenialReasons_.remove(layerId);
    }

    void processLockDenied(const QString& layerId, const QString& reason) {
        pendingLockRequests_.remove(layerId);
        lockDenialReasons_.insert(layerId, reason);
    }

    void processLockReleased(const QString& layerId) {
        locks_.remove(layerId);
    }

    // Marks a local intent to lock; the grant/denial arrives from the server.
    void requestLocalLock(const QString& layerId) {
        pendingLockRequests_.insert(layerId);
    }
    void releaseLocalLock(const QString& layerId) {
        const auto it = locks_.constFind(layerId);
        if (it != locks_.constEnd() && it->clientId == local_.clientId) {
            locks_.erase(it);
        }
        pendingLockRequests_.remove(layerId);
    }

    [[nodiscard]] bool isLayerLocked(const QString& layerId) const {
        return locks_.contains(layerId);
    }

    // True when another peer holds the lock (local locks never block).
    [[nodiscard]] bool isLayerLockedByOther(const QString& layerId) const {
        const auto it = locks_.constFind(layerId);
        return it != locks_.constEnd() && it->clientId != local_.clientId;
    }

    [[nodiscard]] CollabLayerLock lockOwner(const QString& layerId) const {
        return locks_.value(layerId);
    }

    [[nodiscard]] Array<CollabLayerLock> activeLocks() const {
        Array<CollabLayerLock> result;
        for (auto it = locks_.constBegin(); it != locks_.constEnd(); ++it) {
            result.append(it.value());
        }
        return result;
    }

    [[nodiscard]] bool hasPendingLockRequest(const QString& layerId) const {
        return pendingLockRequests_.contains(layerId);
    }

    [[nodiscard]] QString lockDenialReason(const QString& layerId) const {
        return lockDenialReasons_.value(layerId);
    }

private:
    [[nodiscard]] qint64 nextRemoteVersion() const {
        return lastRemoteVersion_ + 1;
    }

    CollabParticipant local_;
    QHash<QString, CollabParticipant> participants_;
    QHash<QString, CollabLayerLock> locks_;
    QSet<QString> pendingLockRequests_;
    QHash<QString, QString> lockDenialReasons_;
    Array<CollabOperationData> operationLog_;
    QSet<QString> seenKeys_;
    qint64 lastRemoteVersion_ = -1;
    qint64 localOperationSequence_ = 0;
};

} // namespace ArtifactCore
