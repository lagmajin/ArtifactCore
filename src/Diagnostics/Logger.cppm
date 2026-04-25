module;
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
#include <wobjectimpl.h>

module Diagnostics.Logger;

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

Logger::Logger(QObject* parent) : QObject(parent) {}

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
    return logs_;
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
            writeLineToLogFile(formatLogLine(logMsg));
        }
    }

    Q_EMIT logAdded(static_cast<int>(level), message, context, logMsg.timestamp);
}

bool Logger::ensureLogFileReady()
{
    if (fileLoggingEnabled_ && logFile_.isOpen()) {
        return true;
    }

    logFilePath_ = defaultLogFilePath();
    QDir dir = QFileInfo(logFilePath_).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        fileLoggingEnabled_ = false;
        return false;
    }

    logFile_.setFileName(logFilePath_);
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
