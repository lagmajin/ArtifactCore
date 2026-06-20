module;
#include <utility>
#include <vector>
#include <chrono>
#include <QString>
#include <QDateTime>
#include <QRandomGenerator>

export module Core.Diagnostics.SessionLedger;

export namespace ArtifactCore {

enum class SessionEntryKind {
    ProjectOpened,
    ProjectClosed,
    ProjectSaved,
    RenderStarted,
    RenderCompleted,
    RenderFailed,
    Crash,
    RecoveryPoint,
    SettingsChanged
};

inline QString sessionEntryKindToString(SessionEntryKind kind) {
    switch (kind) {
    case SessionEntryKind::ProjectOpened:    return QStringLiteral("project.opened");
    case SessionEntryKind::ProjectClosed:    return QStringLiteral("project.closed");
    case SessionEntryKind::ProjectSaved:     return QStringLiteral("project.saved");
    case SessionEntryKind::RenderStarted:    return QStringLiteral("render.started");
    case SessionEntryKind::RenderCompleted:  return QStringLiteral("render.completed");
    case SessionEntryKind::RenderFailed:     return QStringLiteral("render.failed");
    case SessionEntryKind::Crash:            return QStringLiteral("crash");
    case SessionEntryKind::RecoveryPoint:    return QStringLiteral("recovery.point");
    case SessionEntryKind::SettingsChanged:  return QStringLiteral("settings.changed");
    }
    return QStringLiteral("unknown");
}

struct SessionLedgerEntry {
    SessionEntryKind kind = SessionEntryKind::ProjectOpened;
    qint64 timestampMs = 0;
    QString detail;
    QString projectId;
    QString projectName;
    int jobIndex = -1;
    bool isRecoverable = false;
};

struct RecoveryPoint {
    QString id;
    qint64 timestampMs = 0;
    QString projectId;
    QString projectName;
    QString snapshotPath;
    bool isAutosave = false;
};

class SessionLedger {
public:
    SessionLedger()
        : sessionId_(createSessionId())
        , startTimeMs_(currentTimestampMs())
    {}

    void addEntry(const SessionLedgerEntry& entry) {
        entries_.push_back(entry);
    }

    void recordProjectOpened(const QString& projectId, const QString& projectName) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::ProjectOpened;
        e.timestampMs = currentTimestampMs();
        e.projectId = projectId;
        e.projectName = projectName;
        addEntry(e);
    }

    void recordProjectClosed(const QString& projectId) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::ProjectClosed;
        e.timestampMs = currentTimestampMs();
        e.projectId = projectId;
        addEntry(e);
    }

    void recordProjectSaved(const QString& projectId, const QString& projectName) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::ProjectSaved;
        e.timestampMs = currentTimestampMs();
        e.projectId = projectId;
        e.projectName = projectName;
        addEntry(e);
    }

    void recordRenderStarted(int jobIndex, const QString& jobName) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::RenderStarted;
        e.timestampMs = currentTimestampMs();
        e.jobIndex = jobIndex;
        e.detail = jobName;
        addEntry(e);
    }

    void recordRenderCompleted(int jobIndex, const QString& outputPath) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::RenderCompleted;
        e.timestampMs = currentTimestampMs();
        e.jobIndex = jobIndex;
        e.detail = outputPath;
        addEntry(e);
    }

    void recordRenderFailed(int jobIndex, const QString& reason) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::RenderFailed;
        e.timestampMs = currentTimestampMs();
        e.jobIndex = jobIndex;
        e.detail = reason;
        e.isRecoverable = true;
        addEntry(e);
    }

    void recordCrash(const QString& reason) {
        SessionLedgerEntry e;
        e.kind = SessionEntryKind::Crash;
        e.timestampMs = currentTimestampMs();
        e.detail = reason;
        addEntry(e);
    }

    void addRecoveryPoint(const RecoveryPoint& point) {
        recoveryPoints_.push_back(point);
    }

    const QString& sessionId() const { return sessionId_; }
    qint64 startTimeMs() const { return startTimeMs_; }
    const std::vector<SessionLedgerEntry>& entries() const { return entries_; }
    const std::vector<RecoveryPoint>& recoveryPoints() const { return recoveryPoints_; }

    std::vector<SessionLedgerEntry> recoverableEntries() const {
        std::vector<SessionLedgerEntry> result;
        for (const auto& e : entries_) {
            if (e.isRecoverable) {
                result.push_back(e);
            }
        }
        return result;
    }

    void clear() {
        entries_.clear();
        recoveryPoints_.clear();
    }

private:
    static QString createSessionId() {
        const auto now = currentTimestampMs();
        const auto random = QRandomGenerator::global()->generate64();
        return QStringLiteral("%1-%2")
            .arg(now)
            .arg(QString::number(static_cast<qulonglong>(random), 16));
    }

    static qint64 currentTimestampMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    QString sessionId_;
    qint64 startTimeMs_ = 0;
    std::vector<SessionLedgerEntry> entries_;
    std::vector<RecoveryPoint> recoveryPoints_;
};

} // namespace ArtifactCore
