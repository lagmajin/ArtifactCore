module;
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <mutex>
#include <QObject>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <thread>
#include <algorithm>
#include <functional>
#include <wobjectimpl.h>

module Diagnostics.Logger;

import Container.NamedVector;

namespace ArtifactCore {

W_OBJECT_IMPL(Logger)

static QtMessageHandler s_originalHandler = nullptr;

static QString levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return QStringLiteral("DEBUG");
    case LogLevel::Info:
        return QStringLiteral("INFO");
    case LogLevel::Warning:
        return QStringLiteral("WARN");
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    case LogLevel::Fatal:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("DEBUG");
}

static QString defaultLogFilePath()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logDir = QDir(appData).filePath(QStringLiteral("Logs"));
    return QDir(logDir).filePath(QStringLiteral("artifact.log"));
}

static void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    LogLevel level = LogLevel::Debug;
    switch (type) {
    case QtDebugMsg:
        level = LogLevel::Debug;
        break;
    case QtInfoMsg:
        level = LogLevel::Info;
        break;
    case QtWarningMsg:
        level = LogLevel::Warning;
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = LogLevel::Error;
        break;
    }
    
    QString ctxStr;
    if (context.file || context.line || context.function) {
        ctxStr = QString("%1:%2 %3")
            .arg(context.file ? context.file : "")
            .arg(context.line)
            .arg(context.function ? context.function : "");
    }

    Logger::instance()->appendLog(level, msg, ctxStr);

    if (s_originalHandler) {
        s_originalHandler(type, context, msg);
    }
}

Logger* Logger::instance() {
    static Logger logger;
    return &logger;
}

Logger::Logger(QObject* parent) : QObject(parent) {
    for (auto& enabled : categoryEnabled_) enabled.store(true, std::memory_order_relaxed);
}

Logger::~Logger() {
    uninstall();
}

void Logger::install() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!installed_) {
        // Install Qt message handler
        s_originalHandler = qInstallMessageHandler(myMessageOutput);
        ensureLogFileReady();
        if (fileLoggingEnabled_ && logFile_.isOpen()) {
            const QString appName = QCoreApplication::applicationName().isEmpty()
                                        ? QStringLiteral("Artifact")
                                        : QCoreApplication::applicationName();
            const QString header = QStringLiteral("=== %1 log session started at %2 ===")
                                       .arg(appName,
                                            QDateTime::currentDateTime().toString(
                                                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
            writeLineToLogFile(header);
            if (!logFilePath_.isEmpty()) {
                writeLineToLogFile(QStringLiteral("Log file: %1").arg(logFilePath_));
            }
        }
        installed_ = true;
    }
}

void Logger::uninstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (installed_) {
        if (fileLoggingEnabled_ && logFile_.isOpen()) {
            writeLineToLogFile(QStringLiteral("=== log session ended at %1 ===")
                                          .arg(QDateTime::currentDateTime().toString(
                                              QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))));
            logFile_.close();
        }
        qInstallMessageHandler(s_originalHandler);
        s_originalHandler = nullptr;
        installed_ = false;
        fileLoggingEnabled_ = false;
    }
}

std::vector<LogMessage> Logger::getLogs() const {
    // Return a copy so callers never observe the live vector after the lock is released.
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LogMessage> result;
    result.insert(result.end(), logs_.begin(), logs_.end());
    return result;
}

void Logger::clearLogs() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.clear();
    }
    Q_EMIT logsCleared();
}

void Logger::appendLog(LogLevel level, const QString& message, const QString& context) {
    LogMessage logMsg;
    logMsg.timestamp = QDateTime::currentDateTime();
    logMsg.level = level;
    logMsg.message = message;
    logMsg.context = context;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.push_back(logMsg);
        
        if (logs_.size() > 5000) {
            logs_.erase(logs_.begin(), logs_.begin() + 1000);
        }

        if (fileLoggingEnabled_) {
            if (logFileFormat_ == LogFileFormat::JsonLines) {
                QJsonObject json;
                json.insert(QStringLiteral("timestamp"), logMsg.timestamp.toString(Qt::ISODateWithMs));
                json.insert(QStringLiteral("level"), levelName(logMsg.level));
                json.insert(QStringLiteral("message"), logMsg.message);
                if (!logMsg.context.isEmpty()) json.insert(QStringLiteral("context"), logMsg.context);
                writeLineToLogFile(QString::fromUtf8(
                    QJsonDocument(json).toJson(QJsonDocument::Compact)));
            } else {
                writeLineToLogFile(formatLogLine(logMsg));
            }
        }
    }

    Q_EMIT logAdded(static_cast<int>(level), message, context, logMsg.timestamp);
}

void Logger::setFileLoggingEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    fileLoggingRequested_ = enabled;
    if (enabled) {
        if (fileLoggingRequested_) ensureLogFileReady();
    } else if (logFile_.isOpen()) {
        logFile_.flush();
        logFile_.close();
        fileLoggingEnabled_ = false;
    }
}

void Logger::setLogFilePath(const QString& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.isOpen()) logFile_.close();
    logFilePath_ = path;
    fileLoggingEnabled_ = false;
    if (installed_ && fileLoggingRequested_ && !logFilePath_.isEmpty()) ensureLogFileReady();
}

QString Logger::logFilePath() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return logFilePath_;
}

void Logger::setMaxLogFileBytes(std::uint64_t bytes) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    maxLogFileBytes_ = std::max<std::uint64_t>(bytes, 64ull * 1024ull);
}

void Logger::setLogFileFormat(LogFileFormat format) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    logFileFormat_ = format;
}

LogFileFormat Logger::logFileFormat() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return logFileFormat_;
}

void Logger::flushFile()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.isOpen()) logFile_.flush();
}

static QString categoryName(LogCategory category)
{
    switch (category) {
    case LogCategory::General: return QStringLiteral("General");
    case LogCategory::App: return QStringLiteral("App");
    case LogCategory::Project: return QStringLiteral("Project");
    case LogCategory::Timeline: return QStringLiteral("Timeline");
    case LogCategory::RenderVP: return QStringLiteral("Render.VP");
    case LogCategory::RenderGPU: return QStringLiteral("Render.GPU");
    case LogCategory::RenderPass: return QStringLiteral("Render.Pass");
    case LogCategory::RenderResource: return QStringLiteral("Render.Resource");
    case LogCategory::MediaDecode: return QStringLiteral("Media.Decode");
    case LogCategory::NetworkFarm: return QStringLiteral("Network.Farm");
    case LogCategory::ScriptRuntime: return QStringLiteral("Script.Runtime");
    case LogCategory::Diagnostics: return QStringLiteral("Diagnostics");
    }
    return QStringLiteral("General");
}

bool Logger::tryFastLog(LogLevel level, LogCategory category, const char* message,
                        std::uint32_t frame) noexcept
{
    if (!message) return false;
    if (!isCategoryEnabled(category)) return false;
    while (fastLogProducerLock_.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    const auto write = fastLogWrite_.load(std::memory_order_relaxed);
    const auto read = fastLogRead_.load(std::memory_order_acquire);
    if (write - read >= fastLogCapacity_) {
        fastLogDropped_.fetch_add(1, std::memory_order_relaxed);
        fastLogProducerLock_.clear(std::memory_order_release);
        return false;
    }

    auto& record = fastLogBuffer_[write % fastLogCapacity_];
    record.timestampTicks = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    record.category = static_cast<std::uint32_t>(category);
    record.threadId = static_cast<std::uint32_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    record.frame = frame;
    record.level = static_cast<std::uint16_t>(level);
    const int written = std::snprintf(record.message.data(), record.message.size(), "%s", message);
    record.length = static_cast<std::uint16_t>(written > 0
        ? std::min<int>(written, static_cast<int>(record.message.size() - 1)) : 0);
    fastLogWrite_.store(write + 1, std::memory_order_release);
    fastLogProducerLock_.clear(std::memory_order_release);
    return true;
}

bool Logger::tryFastLogFormat(LogLevel level, LogCategory category, std::uint32_t frame,
                              const char* format, ...) noexcept
{
    if (!format || !isCategoryEnabled(category)) return false;

    char message[192]{};
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (written < 0) return false;
    return tryFastLog(level, category, message, frame);
}

void Logger::setCategoryEnabled(LogCategory category, bool enabled) noexcept
{
    const auto index = static_cast<std::size_t>(category);
    if (index < categoryEnabled_.size()) {
        categoryEnabled_[index].store(enabled, std::memory_order_relaxed);
    }
}

bool Logger::isCategoryEnabled(LogCategory category) const noexcept
{
    const auto index = static_cast<std::size_t>(category);
    return index < categoryEnabled_.size()
        && categoryEnabled_[index].load(std::memory_order_relaxed);
}

std::size_t Logger::drainFastLogs(std::size_t maxRecords)
{
    std::size_t drained = 0;
    while (drained < maxRecords) {
        const auto read = fastLogRead_.load(std::memory_order_relaxed);
        const auto write = fastLogWrite_.load(std::memory_order_acquire);
        if (read >= write) break;
        const auto& record = fastLogBuffer_[read % fastLogCapacity_];
        const QString message = QString::fromUtf8(record.message.data(), record.length);
        const QString context = QStringLiteral("category=%1 thread=%2 frame=%3")
            .arg(categoryName(static_cast<LogCategory>(record.category)))
            .arg(record.threadId).arg(record.frame == 0xffffffffu ? -1 : record.frame);
        appendLog(static_cast<LogLevel>(record.level), message, context);
        fastLogRead_.store(read + 1, std::memory_order_release);
        ++drained;
    }
    return drained;
}

std::uint64_t Logger::droppedFastLogCount() const noexcept
{
    return fastLogDropped_.load(std::memory_order_relaxed);
}

void Logger::appendDiagnostic(const DiagnosticEvent& event)
{
    LogLevel level = LogLevel::Info;
    switch (event.severity) {
    case CoreDiagnosticSeverity::Info: level = LogLevel::Info; break;
    case CoreDiagnosticSeverity::Warning: level = LogLevel::Warning; break;
    case CoreDiagnosticSeverity::Error: level = LogLevel::Error; break;
    case CoreDiagnosticSeverity::Fatal: level = LogLevel::Fatal; break;
    }

    const QString message = QStringLiteral("[%1] %2")
        .arg(QString::fromStdString(event.code),
             QString::fromStdString(event.message));
    const QString context = QStringLiteral(
        "%1/%2 object=%3 seq=%6 frame=%7 trace=%8 durationNs=%9 (%4:%5 function=%10)")
        .arg(QString::fromStdString(event.component),
             QString::fromStdString(event.operation),
             QString::fromStdString(event.objectId),
             event.location.file ? QString::fromUtf8(event.location.file) : QString(),
             QString::number(event.location.line),
             QString::number(event.sequence),
             QString::number(event.frameIndex),
             QString::number(event.traceId),
             QString::number(event.durationNs),
             event.location.function ? QString::fromUtf8(event.location.function) : QString());
    appendLog(level, message, context);
}

void Logger::appendDiagnostics(const DiagnosticSnapshot& snapshot)
{
    if (snapshot.eventsTruncated) {
        const QString context = QStringLiteral("firstSeq=%1 lastSeq=%2")
            .arg(QString::number(snapshot.firstSequence),
                 QString::number(snapshot.lastSequence));
        appendLog(LogLevel::Warning,
                  QStringLiteral("[diagnostics.truncated] event history was truncated"),
                  context);
    }
    for (const auto& event : snapshot.recentEvents) {
        appendDiagnostic(event);
    }
}

void Logger::flushDiagnostics(DiagnosticRecorder& recorder)
{
    appendDiagnostics(recorder.drainSnapshot("CoreDiagnostics"));
}

void Logger::recordDiagnostic(LogLevel level,
                               const QString& message,
                               const QString& context)
{
    if (!DiagnosticRecorder::instance().isEnabled()) {
        return;
    }

    CoreDiagnosticSeverity severity = CoreDiagnosticSeverity::Info;
    switch (level) {
    case LogLevel::Debug:
    case LogLevel::Info:
        severity = CoreDiagnosticSeverity::Info;
        break;
    case LogLevel::Warning:
        severity = CoreDiagnosticSeverity::Warning;
        break;
    case LogLevel::Error:
    case LogLevel::Fatal:
        severity = CoreDiagnosticSeverity::Error;
        break;
    }

    const QString code = QStringLiteral("qt.log.%1")
        .arg(QString::fromUtf8(diagnosticSeverityName(severity)));

    const std::string contextStd = context.toStdString();
    auto event = makeDiagnosticEvent(
        severity,
        code.toStdString(),
        message.toStdString(),
        "QtLogger",
        contextStd.empty() ? std::string("log") : contextStd,
        {},
        {});

    DiagnosticRecorder::instance().record(std::move(event));
}

bool Logger::ensureLogFileReady()
{
    if (fileLoggingEnabled_ && logFile_.isOpen()) {
        return true;
    }

    if (logFilePath_.isEmpty()) logFilePath_ = defaultLogFilePath();
    QDir dir = QFileInfo(logFilePath_).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        fileLoggingEnabled_ = false;
        return false;
    }

    logFile_.setFileName(logFilePath_);
    if (QFileInfo::exists(logFilePath_)
        && static_cast<std::uint64_t>(QFileInfo(logFilePath_).size()) >= maxLogFileBytes_) {
        const QString rotatedPath = logFilePath_ + QStringLiteral(".1");
        QFile::remove(rotatedPath);
        QFile::rename(logFilePath_, rotatedPath);
    }
    if (!logFile_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        fileLoggingEnabled_ = false;
        return false;
    }

    fileLoggingEnabled_ = true;
    return true;
}

void Logger::writeLineToLogFile(const QString& line)
{
    if (!fileLoggingEnabled_ || !logFile_.isOpen()) {
        return;
    }

    const QByteArray utf8 = line.toUtf8();
    if (logFile_.write(utf8) < 0 || logFile_.write("\n") < 0) {
        fileLoggingEnabled_ = false;
        logFile_.close();
    } else {
        logFile_.flush();
    }
}

QString Logger::formatLogLine(const LogMessage& logMsg) const
{
    const QString timestamp = logMsg.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    if (logMsg.context.isEmpty()) {
        return QStringLiteral("[%1][%2] %3")
            .arg(timestamp, levelName(logMsg.level), logMsg.message);
    }
    return QStringLiteral("[%1][%2] %3 (%4)")
        .arg(timestamp, levelName(logMsg.level), logMsg.message, logMsg.context);
}

} // namespace ArtifactCore
