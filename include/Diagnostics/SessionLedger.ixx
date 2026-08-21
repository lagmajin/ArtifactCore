module;
#include <utility>
#include <vector>
#include <chrono>
#include <QString>
#include <QDateTime>
#include <QRandomGenerator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

export module Core.Diagnostics.SessionLedger;

import Container.NamedVector;

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
        NamedVector<SessionLedgerEntry> result;
        for (const auto& e : entries_) {
            if (e.isRecoverable) {
                result.push_back(e);
            }
        }
        return result.toStdVector();
    }

    QJsonObject toJson() const {
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        root.insert(QStringLiteral("sessionId"), sessionId_);
        root.insert(QStringLiteral("startTimeMs"), startTimeMs_);
        QJsonArray entries;
        for (const auto& entry : entries_) {
            QJsonObject value;
            value.insert(QStringLiteral("kind"), sessionEntryKindToString(entry.kind));
            value.insert(QStringLiteral("timestampMs"), entry.timestampMs);
            value.insert(QStringLiteral("detail"), entry.detail);
            value.insert(QStringLiteral("projectId"), entry.projectId);
            value.insert(QStringLiteral("projectName"), entry.projectName);
            value.insert(QStringLiteral("jobIndex"), entry.jobIndex);
            value.insert(QStringLiteral("isRecoverable"), entry.isRecoverable);
            entries.append(value);
        }
        root.insert(QStringLiteral("entries"), entries);
        QJsonArray recoveryPoints;
        for (const auto& point : recoveryPoints_) {
            QJsonObject value;
            value.insert(QStringLiteral("id"), point.id);
            value.insert(QStringLiteral("timestampMs"), point.timestampMs);
            value.insert(QStringLiteral("projectId"), point.projectId);
            value.insert(QStringLiteral("projectName"), point.projectName);
            value.insert(QStringLiteral("snapshotPath"), point.snapshotPath);
            value.insert(QStringLiteral("isAutosave"), point.isAutosave);
            recoveryPoints.append(value);
        }
        root.insert(QStringLiteral("recoveryPoints"), recoveryPoints);
        return root;
    }

    bool saveToFile(const QString& path) const {
        if (path.isEmpty()) return false;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QByteArray data = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
        return file.write(data) == data.size() && file.flush();
    }

    bool loadFromFile(const QString& path) {
        if (path.isEmpty()) return false;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
        const QJsonObject root = document.object();
        if (root.value(QStringLiteral("schemaVersion")).toInt() != 1) return false;
        const QString loadedSessionId = root.value(QStringLiteral("sessionId")).toString();
        if (loadedSessionId.isEmpty()) return false;

        NamedVector<SessionLedgerEntry> loadedEntries{
            makeNamedVector<SessionLedgerEntry>(ContainerName{"SessionLedgerLoadedEntries"})};
        for (const auto& item : root.value(QStringLiteral("entries")).toArray()) {
            const QJsonObject value = item.toObject();
            SessionLedgerEntry entry;
            entry.kind = sessionEntryKindFromString(value.value(QStringLiteral("kind")).toString());
            entry.timestampMs = value.value(QStringLiteral("timestampMs")).toVariant().toLongLong();
            entry.detail = value.value(QStringLiteral("detail")).toString();
            entry.projectId = value.value(QStringLiteral("projectId")).toString();
            entry.projectName = value.value(QStringLiteral("projectName")).toString();
            entry.jobIndex = value.value(QStringLiteral("jobIndex")).toInt(-1);
            entry.isRecoverable = value.value(QStringLiteral("isRecoverable")).toBool(false);
            loadedEntries.append(std::move(entry));
        }
        NamedVector<RecoveryPoint> loadedRecoveryPoints{
            makeNamedVector<RecoveryPoint>(ContainerName{"SessionLedgerLoadedRecoveryPoints"})};
        for (const auto& item : root.value(QStringLiteral("recoveryPoints")).toArray()) {
            const QJsonObject value = item.toObject();
            RecoveryPoint point;
            point.id = value.value(QStringLiteral("id")).toString();
            point.timestampMs = value.value(QStringLiteral("timestampMs")).toVariant().toLongLong();
            point.projectId = value.value(QStringLiteral("projectId")).toString();
            point.projectName = value.value(QStringLiteral("projectName")).toString();
            point.snapshotPath = value.value(QStringLiteral("snapshotPath")).toString();
            point.isAutosave = value.value(QStringLiteral("isAutosave")).toBool(false);
            loadedRecoveryPoints.append(std::move(point));
        }
        sessionId_ = loadedSessionId;
        startTimeMs_ = root.value(QStringLiteral("startTimeMs")).toVariant().toLongLong();
        entries_.clear();
        entries_.insert(entries_.end(), loadedEntries.begin(), loadedEntries.end());
        recoveryPoints_.clear();
        recoveryPoints_.insert(
            recoveryPoints_.end(), loadedRecoveryPoints.begin(), loadedRecoveryPoints.end());
        return true;
    }

    void clear() {
        entries_.clear();
        recoveryPoints_.clear();
    }

private:
    static SessionEntryKind sessionEntryKindFromString(const QString& value) {
        if (value == QStringLiteral("project.closed")) return SessionEntryKind::ProjectClosed;
        if (value == QStringLiteral("project.saved")) return SessionEntryKind::ProjectSaved;
        if (value == QStringLiteral("render.started")) return SessionEntryKind::RenderStarted;
        if (value == QStringLiteral("render.completed")) return SessionEntryKind::RenderCompleted;
        if (value == QStringLiteral("render.failed")) return SessionEntryKind::RenderFailed;
        if (value == QStringLiteral("crash")) return SessionEntryKind::Crash;
        if (value == QStringLiteral("recovery.point")) return SessionEntryKind::RecoveryPoint;
        if (value == QStringLiteral("settings.changed")) return SessionEntryKind::SettingsChanged;
        return SessionEntryKind::ProjectOpened;
    }

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
