module;
#include <utility>
#include <QString>
#include <QDateTime>
#include <vector>
#include <mutex>
#include <functional>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Log;

import Render.Farm.Types;

export namespace ArtifactCore {

// Log severity levels for worker diagnostics
enum class LogSeverity {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

// A single structured log entry from a worker
struct LogEntry {
    QString workerId;
    QDateTime timestamp;
    LogSeverity severity = LogSeverity::Info;
    QString message;
    int frame = -1;
    QString category;
};

// Query filter for retrieving log entries
struct LogQuery {
    std::vector<QString> workerIds;
    std::vector<LogSeverity> severities;
    QDateTime fromTime;
    QDateTime toTime;
    int maxResults = 1000;
    int frame = -1;
    std::vector<QString> categories;
};

class LIBRARY_DLL_API LogCollector {
public:
    LogCollector();
    ~LogCollector();

    // Set the log directory (e.g. "<project_root>/.artifact/farm/logs/")
    void setLogDirectory(const QString& path);
    QString logDirectory() const;

    // Append a single log entry
    void append(const LogEntry& entry);

    // Convenience: log a message with severity
    void log(const QString& workerId, LogSeverity severity,
             const QString& message, int frame = -1,
             const QString& category = "");

    void logInfo(const QString& workerId, const QString& message, int frame = -1);
    void logWarning(const QString& workerId, const QString& message, int frame = -1);
    void logError(const QString& workerId, const QString& message, int frame = -1);
    void logDebug(const QString& workerId, const QString& message, int frame = -1);

    // Flush buffered entries to disk
    void flush();

    // Retrieve entries matching a query (from in-memory buffer)
    std::vector<LogEntry> query(const LogQuery& query) const;

    // Clear all in-memory entries
    void clear();

    // Total entries collected
    size_t totalEntries() const;

    // Set a callback for real-time log monitoring
    using LogCallback = std::function<void(const LogEntry&)>;
    void setLogCallback(LogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}

