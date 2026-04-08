module;
#include <utility>
#include <vector>
#include <mutex>
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QDebug>
#include <wobjectimpl.h>

module Diagnostics.Logger;

namespace ArtifactCore {

W_OBJECT_IMPL(Logger)

static QtMessageHandler s_originalHandler = nullptr;

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
        installed_ = true;
    }
}

void Logger::uninstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (installed_) {
        qInstallMessageHandler(s_originalHandler);
        s_originalHandler = nullptr;
        installed_ = false;
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
    }

    Q_EMIT logAdded(static_cast<int>(level), message, context, logMsg.timestamp);
}

} // namespace ArtifactCore
