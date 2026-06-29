module;
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>
#include <deque>
#include <fstream>

module Render.Farm.Log;

import std;

namespace ArtifactCore {

struct LogCollector::Impl {
    std::mutex mutex;
    std::deque<LogEntry> entries;
    QString logDir;
    size_t flushCounter = 0;
    size_t flushThreshold = 100;
    LogCollector::LogCallback callback;

    void appendEntry(const LogEntry& entry) {
        std::lock_guard lock(mutex);
        entries.push_back(entry);
        if (callback) {
            callback(entry);
        }
        flushCounter++;
        if (flushCounter >= flushThreshold) {
            flushCounter = 0;
            // schedule flush — we do it in the next writeLogFile call
        }
    }

    void writeLogFile() {
        if (logDir.isEmpty()) return;

        QDir dir(logDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        std::lock_guard lock(mutex);

        if (entries.empty()) return;

        QString filePath = dir.filePath("farm_log.jsonl");
        QFile file(filePath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            while (!entries.empty()) {
                const auto& e = entries.front();
                QJsonObject obj;
                obj["workerId"] = e.workerId;
                obj["timestamp"] = e.timestamp.toString(Qt::ISODateWithMs);
                obj["severity"] = static_cast<int>(e.severity);
                obj["message"] = e.message;
                if (e.frame >= 0) obj["frame"] = e.frame;
                if (!e.category.isEmpty()) obj["category"] = e.category;
                QJsonDocument doc(obj);
                stream << QString::fromUtf8(doc.toJson(QJsonDocument::Compact)) << "\n";
                entries.pop_front();
            }
        } else {
            qWarning() << "[LogCollector] Failed to open log file for writing:" << filePath;
        }
    }
};

LogCollector::LogCollector()
    : impl_(std::make_unique<Impl>())
{
}

LogCollector::~LogCollector() {
    flush();
}

void LogCollector::setLogDirectory(const QString& path) {
    impl_->logDir = path;
}

QString LogCollector::logDirectory() const {
    return impl_->logDir;
}

void LogCollector::append(const LogEntry& entry) {
    impl_->appendEntry(entry);
}

void LogCollector::log(const QString& workerId, LogSeverity severity,
                       const QString& message, int frame, const QString& category)
{
    LogEntry entry;
    entry.workerId = workerId;
    entry.timestamp = QDateTime::currentDateTime();
    entry.severity = severity;
    entry.message = message;
    entry.frame = frame;
    entry.category = category;
    impl_->appendEntry(entry);
}

void LogCollector::logInfo(const QString& workerId, const QString& message, int frame) {
    log(workerId, LogSeverity::Info, message, frame);
}

void LogCollector::logWarning(const QString& workerId, const QString& message, int frame) {
    log(workerId, LogSeverity::Warning, message, frame);
}

void LogCollector::logError(const QString& workerId, const QString& message, int frame) {
    log(workerId, LogSeverity::Error, message, frame);
}

void LogCollector::logDebug(const QString& workerId, const QString& message, int frame) {
    log(workerId, LogSeverity::Debug, message, frame);
}

void LogCollector::flush() {
    impl_->writeLogFile();
}

std::vector<LogEntry> LogCollector::query(const LogQuery& query) const {
    std::lock_guard lock(impl_->mutex);

    std::vector<LogEntry> result;
    for (const auto& entry : impl_->entries) {
        // Filter worker IDs
        if (!query.workerIds.empty()) {
            bool match = false;
            for (const auto& wid : query.workerIds) {
                if (entry.workerId == wid) { match = true; break; }
            }
            if (!match) continue;
        }

        // Filter severities
        if (!query.severities.empty()) {
            bool match = false;
            for (const auto& sev : query.severities) {
                if (entry.severity == sev) { match = true; break; }
            }
            if (!match) continue;
        }

        // Filter time range
        if (query.fromTime.isValid() && entry.timestamp < query.fromTime) continue;
        if (query.toTime.isValid() && entry.timestamp > query.toTime) continue;

        // Filter frame
        if (query.frame >= 0 && entry.frame != query.frame) continue;

        // Filter categories
        if (!query.categories.empty()) {
            bool match = false;
            for (const auto& cat : query.categories) {
                if (entry.category == cat) { match = true; break; }
            }
            if (!match) continue;
        }

        result.push_back(entry);
        if (static_cast<int>(result.size()) >= query.maxResults) break;
    }

    return result;
}

void LogCollector::clear() {
    std::lock_guard lock(impl_->mutex);
    impl_->entries.clear();
}

size_t LogCollector::totalEntries() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->entries.size();
}

void LogCollector::setLogCallback(LogCallback callback) {
    impl_->callback = std::move(callback);
}

}

