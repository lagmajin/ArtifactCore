module;
class tst_QList;
#include <QString>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>
#include <QHash>
#include <QThread>
#include <QStringList>
#include <cstdint>
#include <mutex>

export module Core.Diagnostics.Trace;

import Frame.Debug;

export namespace ArtifactCore {

enum class TraceDomain : std::uint8_t {
    Crash,
    Scope,
    Timeline,
    Thread,
    Event,
    Render,
    Decode,
    UI
};

struct TraceScopeRecord {
    QString name;
    TraceDomain domain = TraceDomain::Scope;
    qint64 startNs = 0;
    qint64 endNs = 0;
    std::uint64_t threadId = 0;
    int frameIndex = -1;
};

struct TraceCrashRecord {
    QString summary;
    QString stack;
    QString threadName;
    qint64 timestampMs = 0;
    std::uint64_t threadId = 0;
};

struct TraceLockRecord {
    QString mutexName;
    bool acquired = true;
    qint64 timestampNs = 0;
    std::uint64_t threadId = 0;
};

enum class TraceEventKind : std::uint8_t {
    Scope,
    Lock,
    Crash,
    Frame
};

struct TraceEventRecord {
    TraceEventKind kind = TraceEventKind::Scope;
    TraceDomain domain = TraceDomain::Timeline;
    QString name;
    QString detail;
    qint64 startNs = 0;
    qint64 endNs = 0;
    std::uint64_t threadId = 0;
    int frameIndex = -1;
    bool acquired = false;
};

struct TraceThreadRecord {
    std::uint64_t threadId = 0;
    QString threadName;
    int scopeCount = 0;
    int lockCount = 0;
    int crashCount = 0;
    int lockDepth = 0;
    QString lastMutexName;
    bool lastLockAcquired = false;
    qint64 lastLockNs = 0;
};

struct TraceFrameLaneRecord {
    QString laneName;
    QVector<TraceScopeRecord> scopes;
};

struct TraceFrameTimelineRecord {
    int frameIndex = -1;
    qint64 frameStartNs = 0;
    qint64 frameEndNs = 0;
    QVector<TraceFrameLaneRecord> lanes;
};

struct TraceSnapshot {
    QVector<TraceCrashRecord> crashes;
    QVector<TraceScopeRecord> scopes;
    QVector<TraceLockRecord> locks;
    QVector<TraceThreadRecord> threads;
    QVector<TraceFrameTimelineRecord> frames;
    QVector<TraceEventRecord> events;
};

inline QString toString(TraceDomain domain)
{
    switch (domain) {
    case TraceDomain::Crash: return QStringLiteral("Crash");
    case TraceDomain::Scope: return QStringLiteral("Scope");
    case TraceDomain::Timeline: return QStringLiteral("Timeline");
    case TraceDomain::Thread: return QStringLiteral("Thread");
    case TraceDomain::Event: return QStringLiteral("Event");
    case TraceDomain::Render: return QStringLiteral("Render");
    case TraceDomain::Decode: return QStringLiteral("Decode");
    case TraceDomain::UI: return QStringLiteral("UI");
    }
    return QStringLiteral("Scope");
}

inline TraceDomain traceDomainFromString(const QString& value)
{
    if (value == QStringLiteral("Crash")) return TraceDomain::Crash;
    if (value == QStringLiteral("Scope")) return TraceDomain::Scope;
    if (value == QStringLiteral("Timeline")) return TraceDomain::Timeline;
    if (value == QStringLiteral("Thread")) return TraceDomain::Thread;
    if (value == QStringLiteral("Event")) return TraceDomain::Event;
    if (value == QStringLiteral("Render")) return TraceDomain::Render;
    if (value == QStringLiteral("Decode")) return TraceDomain::Decode;
    if (value == QStringLiteral("UI")) return TraceDomain::UI;
    return TraceDomain::Scope;
}

inline QString toString(TraceEventKind kind)
{
    switch (kind) {
    case TraceEventKind::Scope: return QStringLiteral("Scope");
    case TraceEventKind::Lock: return QStringLiteral("Lock");
    case TraceEventKind::Crash: return QStringLiteral("Crash");
    case TraceEventKind::Frame: return QStringLiteral("Frame");
    }
    return QStringLiteral("Scope");
}

inline TraceEventKind traceEventKindFromString(const QString& value)
{
    if (value == QStringLiteral("Scope")) return TraceEventKind::Scope;
    if (value == QStringLiteral("Lock")) return TraceEventKind::Lock;
    if (value == QStringLiteral("Crash")) return TraceEventKind::Crash;
    if (value == QStringLiteral("Frame")) return TraceEventKind::Frame;
    return TraceEventKind::Scope;
}

inline QJsonObject toJson(const TraceScopeRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), record.name);
    json.insert(QStringLiteral("domain"), toString(record.domain));
    json.insert(QStringLiteral("startNs"), static_cast<double>(record.startNs));
    json.insert(QStringLiteral("endNs"), static_cast<double>(record.endNs));
    json.insert(QStringLiteral("threadId"), QString::number(static_cast<unsigned long long>(record.threadId)));
    json.insert(QStringLiteral("frameIndex"), record.frameIndex);
    return json;
}

inline TraceScopeRecord traceScopeRecordFromJson(const QJsonObject& json)
{
    TraceScopeRecord record;
    record.name = json.value(QStringLiteral("name")).toString();
    record.domain = traceDomainFromString(json.value(QStringLiteral("domain")).toString());
    record.startNs = static_cast<qint64>(json.value(QStringLiteral("startNs")).toDouble());
    record.endNs = static_cast<qint64>(json.value(QStringLiteral("endNs")).toDouble());
    record.threadId = json.value(QStringLiteral("threadId")).toString().toULongLong();
    record.frameIndex = json.value(QStringLiteral("frameIndex")).toInt(-1);
    return record;
}

inline QJsonObject toJson(const TraceCrashRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("summary"), record.summary);
    json.insert(QStringLiteral("stack"), record.stack);
    json.insert(QStringLiteral("threadName"), record.threadName);
    json.insert(QStringLiteral("timestampMs"), static_cast<double>(record.timestampMs));
    json.insert(QStringLiteral("threadId"), QString::number(static_cast<unsigned long long>(record.threadId)));
    return json;
}

inline TraceCrashRecord traceCrashRecordFromJson(const QJsonObject& json)
{
    TraceCrashRecord record;
    record.summary = json.value(QStringLiteral("summary")).toString();
    record.stack = json.value(QStringLiteral("stack")).toString();
    record.threadName = json.value(QStringLiteral("threadName")).toString();
    record.timestampMs = static_cast<qint64>(json.value(QStringLiteral("timestampMs")).toDouble());
    record.threadId = json.value(QStringLiteral("threadId")).toString().toULongLong();
    return record;
}

inline QJsonObject toJson(const TraceLockRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("mutexName"), record.mutexName);
    json.insert(QStringLiteral("acquired"), record.acquired);
    json.insert(QStringLiteral("timestampNs"), static_cast<double>(record.timestampNs));
    json.insert(QStringLiteral("threadId"), QString::number(static_cast<unsigned long long>(record.threadId)));
    return json;
}

inline TraceLockRecord traceLockRecordFromJson(const QJsonObject& json)
{
    TraceLockRecord record;
    record.mutexName = json.value(QStringLiteral("mutexName")).toString();
    record.acquired = json.value(QStringLiteral("acquired")).toBool();
    record.timestampNs = static_cast<qint64>(json.value(QStringLiteral("timestampNs")).toDouble());
    record.threadId = json.value(QStringLiteral("threadId")).toString().toULongLong();
    return record;
}

inline QJsonObject toJson(const TraceThreadRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("threadId"), QString::number(static_cast<unsigned long long>(record.threadId)));
    json.insert(QStringLiteral("threadName"), record.threadName);
    json.insert(QStringLiteral("scopeCount"), record.scopeCount);
    json.insert(QStringLiteral("lockCount"), record.lockCount);
    json.insert(QStringLiteral("crashCount"), record.crashCount);
    json.insert(QStringLiteral("lockDepth"), record.lockDepth);
    json.insert(QStringLiteral("lastMutexName"), record.lastMutexName);
    json.insert(QStringLiteral("lastLockAcquired"), record.lastLockAcquired);
    json.insert(QStringLiteral("lastLockNs"), static_cast<double>(record.lastLockNs));
    return json;
}

inline TraceThreadRecord traceThreadRecordFromJson(const QJsonObject& json)
{
    TraceThreadRecord record;
    record.threadId = json.value(QStringLiteral("threadId")).toString().toULongLong();
    record.threadName = json.value(QStringLiteral("threadName")).toString();
    record.scopeCount = json.value(QStringLiteral("scopeCount")).toInt();
    record.lockCount = json.value(QStringLiteral("lockCount")).toInt();
    record.crashCount = json.value(QStringLiteral("crashCount")).toInt();
    record.lockDepth = json.value(QStringLiteral("lockDepth")).toInt();
    record.lastMutexName = json.value(QStringLiteral("lastMutexName")).toString();
    record.lastLockAcquired = json.value(QStringLiteral("lastLockAcquired")).toBool();
    record.lastLockNs = static_cast<qint64>(json.value(QStringLiteral("lastLockNs")).toDouble());
    return record;
}

inline QJsonObject toJson(const TraceFrameLaneRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("laneName"), record.laneName);
    QJsonArray scopesJson;
    for (const auto& scope : record.scopes) {
        scopesJson.append(toJson(scope));
    }
    json.insert(QStringLiteral("scopes"), scopesJson);
    return json;
}

inline TraceFrameLaneRecord traceFrameLaneRecordFromJson(const QJsonObject& json)
{
    TraceFrameLaneRecord record;
    record.laneName = json.value(QStringLiteral("laneName")).toString();
    for (const auto& value : json.value(QStringLiteral("scopes")).toArray()) {
        record.scopes.push_back(traceScopeRecordFromJson(value.toObject()));
    }
    return record;
}

inline QJsonObject toJson(const TraceFrameTimelineRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("frameIndex"), record.frameIndex);
    json.insert(QStringLiteral("frameStartNs"), static_cast<double>(record.frameStartNs));
    json.insert(QStringLiteral("frameEndNs"), static_cast<double>(record.frameEndNs));
    QJsonArray lanesJson;
    for (const auto& lane : record.lanes) {
        lanesJson.append(toJson(lane));
    }
    json.insert(QStringLiteral("lanes"), lanesJson);
    return json;
}

inline TraceFrameTimelineRecord traceFrameTimelineRecordFromJson(const QJsonObject& json)
{
    TraceFrameTimelineRecord record;
    record.frameIndex = json.value(QStringLiteral("frameIndex")).toInt(-1);
    record.frameStartNs = static_cast<qint64>(json.value(QStringLiteral("frameStartNs")).toDouble());
    record.frameEndNs = static_cast<qint64>(json.value(QStringLiteral("frameEndNs")).toDouble());
    for (const auto& value : json.value(QStringLiteral("lanes")).toArray()) {
        record.lanes.push_back(traceFrameLaneRecordFromJson(value.toObject()));
    }
    return record;
}

inline QJsonObject toJson(const TraceEventRecord& record)
{
    QJsonObject json;
    json.insert(QStringLiteral("kind"), toString(record.kind));
    json.insert(QStringLiteral("domain"), toString(record.domain));
    json.insert(QStringLiteral("name"), record.name);
    json.insert(QStringLiteral("detail"), record.detail);
    json.insert(QStringLiteral("startNs"), static_cast<double>(record.startNs));
    json.insert(QStringLiteral("endNs"), static_cast<double>(record.endNs));
    json.insert(QStringLiteral("threadId"), QString::number(static_cast<unsigned long long>(record.threadId)));
    json.insert(QStringLiteral("frameIndex"), record.frameIndex);
    json.insert(QStringLiteral("acquired"), record.acquired);
    return json;
}

inline TraceEventRecord traceEventRecordFromJson(const QJsonObject& json)
{
    TraceEventRecord record;
    record.kind = traceEventKindFromString(json.value(QStringLiteral("kind")).toString());
    record.domain = traceDomainFromString(json.value(QStringLiteral("domain")).toString());
    record.name = json.value(QStringLiteral("name")).toString();
    record.detail = json.value(QStringLiteral("detail")).toString();
    record.startNs = static_cast<qint64>(json.value(QStringLiteral("startNs")).toDouble());
    record.endNs = static_cast<qint64>(json.value(QStringLiteral("endNs")).toDouble());
    record.threadId = json.value(QStringLiteral("threadId")).toString().toULongLong();
    record.frameIndex = json.value(QStringLiteral("frameIndex")).toInt(-1);
    record.acquired = json.value(QStringLiteral("acquired")).toBool();
    return record;
}

inline QJsonObject toJson(const TraceSnapshot& snapshot)
{
    QJsonObject json;
    QJsonArray crashesJson;
    for (const auto& crash : snapshot.crashes) {
        crashesJson.append(toJson(crash));
    }
    json.insert(QStringLiteral("crashes"), crashesJson);

    QJsonArray scopesJson;
    for (const auto& scope : snapshot.scopes) {
        scopesJson.append(toJson(scope));
    }
    json.insert(QStringLiteral("scopes"), scopesJson);

    QJsonArray locksJson;
    for (const auto& lock : snapshot.locks) {
        locksJson.append(toJson(lock));
    }
    json.insert(QStringLiteral("locks"), locksJson);

    QJsonArray threadsJson;
    for (const auto& thread : snapshot.threads) {
        threadsJson.append(toJson(thread));
    }
    json.insert(QStringLiteral("threads"), threadsJson);

    QJsonArray framesJson;
    for (const auto& frame : snapshot.frames) {
        framesJson.append(toJson(frame));
    }
    json.insert(QStringLiteral("frames"), framesJson);

    QJsonArray eventsJson;
    for (const auto& event : snapshot.events) {
        eventsJson.append(toJson(event));
    }
    json.insert(QStringLiteral("events"), eventsJson);
    return json;
}

inline TraceSnapshot traceSnapshotFromJson(const QJsonObject& json)
{
    TraceSnapshot snapshot;
    for (const auto& value : json.value(QStringLiteral("crashes")).toArray()) {
        snapshot.crashes.push_back(traceCrashRecordFromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("scopes")).toArray()) {
        snapshot.scopes.push_back(traceScopeRecordFromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("locks")).toArray()) {
        snapshot.locks.push_back(traceLockRecordFromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("threads")).toArray()) {
        snapshot.threads.push_back(traceThreadRecordFromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("frames")).toArray()) {
        snapshot.frames.push_back(traceFrameTimelineRecordFromJson(value.toObject()));
    }
    for (const auto& value : json.value(QStringLiteral("events")).toArray()) {
        snapshot.events.push_back(traceEventRecordFromJson(value.toObject()));
    }
    return snapshot;
}

class TraceRecorder {
public:
    static constexpr int kMaxCrashRecords = 32;
    static constexpr int kMaxScopeRecords = 2048;
    static constexpr int kMaxLockRecords = 1024;
    static constexpr int kMaxFrameRecords = 120;
    static constexpr int kMaxEventRecords = 4096;

    static TraceRecorder& instance()
    {
        // Intentionally leaked to avoid static-destruction races with worker threads.
        // Trace scopes can still unwind during process shutdown, and a destroyed
        // singleton would turn that into a use-after-free.
        static TraceRecorder* recorder = new TraceRecorder();
        return *recorder;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = {};
        threadIndexById_.clear();
    }

    TraceSnapshot snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    }

    void recordScope(const TraceScopeRecord& scope)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.scopes.push_back(scope);
        snapshot_.events.push_back(makeScopeEvent(scope));
        trimToLimit(snapshot_.scopes, kMaxScopeRecords);
        trimToLimit(snapshot_.events, kMaxEventRecords);
        updateThreadLocked(scope.threadId, QString(), 1, 0, 0);
    }

    void recordCrash(const TraceCrashRecord& crash)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.crashes.push_back(crash);
        snapshot_.events.push_back(makeCrashEvent(crash));
        trimToLimit(snapshot_.crashes, kMaxCrashRecords);
        trimToLimit(snapshot_.events, kMaxEventRecords);
        updateThreadLocked(crash.threadId, crash.threadName, 0, 0, 1);
    }

    void recordLock(const TraceLockRecord& lock)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        snapshot_.locks.push_back(lock);
        snapshot_.events.push_back(makeLockEvent(lock));
        trimToLimit(snapshot_.locks, kMaxLockRecords);
        trimToLimit(snapshot_.events, kMaxEventRecords);
        updateThreadLocked(lock.threadId, QString(), 0, 1, 0);
        updateLockChainLocked(lock.threadId, lock.mutexName, lock.acquired, lock.timestampNs);
    }

    void recordLock(const QString& mutexName, bool acquired)
    {
        TraceLockRecord lock;
        lock.mutexName = mutexName;
        lock.acquired = acquired;
        lock.timestampNs = currentTimeNs();
        lock.threadId = currentThreadId();
        recordLock(lock);
    }

    void recordThread(std::uint64_t threadId, const QString& threadName = QString())
    {
        std::lock_guard<std::mutex> lock(mutex_);
        updateThreadLocked(threadId, threadName, 0, 0, 0);
    }

    void recordThreadState(const QString& threadName, std::uint64_t threadId = 0)
    {
        if (threadId == 0) {
            threadId = currentThreadId();
        }
        recordThread(threadId, threadName);
    }

    void recordCurrentThread()
    {
        recordThread(currentThreadId(), currentThreadName());
    }

    void recordScope(const QString& name, TraceDomain domain, qint64 startNs, qint64 endNs, int frameIndex = -1)
    {
        TraceScopeRecord scope;
        scope.name = name;
        scope.domain = domain;
        scope.startNs = startNs;
        scope.endNs = endNs;
        scope.threadId = currentThreadId();
        scope.frameIndex = frameIndex;
        recordScope(scope);
    }

    void recordFrame(const TraceFrameTimelineRecord& frame)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.frames.push_back(frame);
        snapshot_.events.push_back(makeFrameEvent(frame));
        trimToLimit(snapshot_.frames, kMaxFrameRecords);
        trimToLimit(snapshot_.events, kMaxEventRecords);
    }

    void recordFrameDebugSnapshot(const FrameDebugSnapshot& snapshot)
    {
        recordFrame(makeFrameTimelineRecord(snapshot));
    }

private:
    static qint64 currentTimeNs()
    {
        return QDateTime::currentMSecsSinceEpoch() * 1000000LL;
    }

    static qint64 currentTimeMs()
    {
        return QDateTime::currentMSecsSinceEpoch();
    }

    static std::uint64_t currentThreadId()
    {
        return static_cast<std::uint64_t>(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    }

    static QString currentThreadName()
    {
        if (auto* thread = QThread::currentThread()) {
            const QString name = thread->objectName().trimmed();
            if (!name.isEmpty()) {
                return name;
            }
        }
        return QStringLiteral("thread-%1").arg(QString::number(static_cast<unsigned long long>(currentThreadId())));
    }

    static TraceEventRecord makeScopeEvent(const TraceScopeRecord& scope)
    {
        TraceEventRecord event;
        event.kind = TraceEventKind::Scope;
        event.domain = scope.domain;
        event.name = scope.name;
        event.detail = QStringLiteral("scope");
        event.startNs = scope.startNs;
        event.endNs = scope.endNs;
        event.threadId = scope.threadId;
        event.frameIndex = scope.frameIndex;
        return event;
    }

    static TraceEventRecord makeLockEvent(const TraceLockRecord& lock)
    {
        TraceEventRecord event;
        event.kind = TraceEventKind::Lock;
        event.domain = TraceDomain::Thread;
        event.name = lock.mutexName;
        event.detail = lock.acquired ? QStringLiteral("acquire") : QStringLiteral("release");
        event.startNs = lock.timestampNs;
        event.endNs = lock.timestampNs;
        event.threadId = lock.threadId;
        return event;
    }

    static TraceEventRecord makeCrashEvent(const TraceCrashRecord& crash)
    {
        TraceEventRecord event;
        event.kind = TraceEventKind::Crash;
        event.domain = TraceDomain::Crash;
        event.name = crash.summary;
        event.detail = crash.threadName;
        event.startNs = crash.timestampMs * 1000000LL;
        event.endNs = event.startNs;
        event.threadId = crash.threadId;
        return event;
    }

    static TraceEventRecord makeFrameEvent(const TraceFrameTimelineRecord& frame)
    {
        TraceEventRecord event;
        event.kind = TraceEventKind::Frame;
        event.domain = TraceDomain::Timeline;
        event.name = QStringLiteral("frame-%1").arg(frame.frameIndex);
        event.detail = QStringLiteral("frame");
        event.startNs = frame.frameStartNs;
        event.endNs = frame.frameEndNs;
        event.frameIndex = frame.frameIndex;
        return event;
    }

    static TraceFrameTimelineRecord makeFrameTimelineRecord(const FrameDebugSnapshot& snapshot)
    {
        TraceFrameTimelineRecord frame;
        frame.frameIndex = snapshot.frame.framePosition();
        frame.frameStartNs = snapshot.timestampMs * 1000000LL;

        qint64 totalDurationUs = 0;
        for (const auto& pass : snapshot.passes) {
            TraceFrameLaneRecord lane;
            lane.laneName = pass.name;
            TraceScopeRecord scope;
            scope.name = pass.name;
            scope.domain = domainFromPassKind(pass.kind);
            scope.startNs = frame.frameStartNs + totalDurationUs * 1000LL;
            scope.endNs = scope.startNs + pass.durationUs * 1000LL;
            scope.threadId = 0;
            scope.frameIndex = frame.frameIndex;
            lane.scopes.push_back(scope);
            frame.lanes.push_back(lane);
            totalDurationUs += pass.durationUs;
        }

        frame.frameEndNs = frame.frameStartNs + totalDurationUs * 1000LL;
        return frame;
    }

    static TraceDomain domainFromPassKind(FrameDebugPassKind kind)
    {
        switch (kind) {
        case FrameDebugPassKind::Clear: return TraceDomain::Render;
        case FrameDebugPassKind::Draw: return TraceDomain::Render;
        case FrameDebugPassKind::Resolve: return TraceDomain::Render;
        case FrameDebugPassKind::Readback: return TraceDomain::Decode;
        case FrameDebugPassKind::Upload: return TraceDomain::Render;
        case FrameDebugPassKind::Composite: return TraceDomain::UI;
        case FrameDebugPassKind::Encode: return TraceDomain::Event;
        default: return TraceDomain::Timeline;
        }
    }

    void updateThreadLocked(std::uint64_t threadId, const QString& threadName,
                            int scopeDelta, int lockDelta, int crashDelta)
    {
        if (threadId == 0) {
            threadId = currentThreadId();
        }

        const auto key = static_cast<quint64>(threadId);
        auto it = threadIndexById_.find(key);
        if (it == threadIndexById_.end()) {
            TraceThreadRecord thread;
            thread.threadId = threadId;
            thread.threadName = threadName.isEmpty() ? currentThreadName() : threadName;
            thread.scopeCount = scopeDelta;
            thread.lockCount = lockDelta;
            thread.crashCount = crashDelta;
            const int index = snapshot_.threads.size();
            snapshot_.threads.push_back(thread);
            threadIndexById_.insert(key, index);
            return;
        }

        const int index = it.value();
        if (index >= 0 && index < snapshot_.threads.size()) {
            auto& thread = snapshot_.threads[index];
            if (thread.threadName.isEmpty()) {
                thread.threadName = threadName.isEmpty() ? currentThreadName() : threadName;
            }
            thread.scopeCount += scopeDelta;
            thread.lockCount += lockDelta;
            thread.crashCount += crashDelta;
        }
    }

    void updateLockChainLocked(std::uint64_t threadId, const QString& mutexName,
                               bool acquired, qint64 timestampNs)
    {
        if (threadId == 0) {
            threadId = currentThreadId();
        }

        const auto key = static_cast<quint64>(threadId);
        auto it = threadIndexById_.find(key);
        if (it == threadIndexById_.end()) {
            TraceThreadRecord thread;
            thread.threadId = threadId;
            thread.threadName = currentThreadName();
            thread.lockDepth = acquired ? 1 : 0;
            thread.lastMutexName = mutexName;
            thread.lastLockAcquired = acquired;
            thread.lastLockNs = timestampNs;
            const int index = snapshot_.threads.size();
            snapshot_.threads.push_back(thread);
            threadIndexById_.insert(key, index);
            return;
        }

        const int index = it.value();
        if (index >= 0 && index < snapshot_.threads.size()) {
            auto& thread = snapshot_.threads[index];
            thread.lockDepth = std::max(0, thread.lockDepth + (acquired ? 1 : -1));
            thread.lastMutexName = mutexName;
            thread.lastLockAcquired = acquired;
            thread.lastLockNs = timestampNs;
        }
    }

    template <typename T>
    static void trimToLimit(QVector<T>& records, int limit)
    {
        while (records.size() > limit) {
            records.removeFirst();
        }
    }

    mutable std::mutex mutex_;
    TraceSnapshot snapshot_;
    QHash<quint64, int> threadIndexById_;
};

class TraceLockScope {
public:
    explicit TraceLockScope(const QString& mutexName)
        : mutexName_(mutexName)
    {
        TraceRecorder::instance().recordLock(mutexName_, true);
    }

    ~TraceLockScope()
    {
        TraceRecorder::instance().recordLock(mutexName_, false);
    }

private:
    QString mutexName_;
};

class TraceThreadScope {
public:
    explicit TraceThreadScope(const QString& threadName = QString())
    {
        TraceRecorder::instance().recordThreadState(threadName);
    }
};

} // namespace ArtifactCore
