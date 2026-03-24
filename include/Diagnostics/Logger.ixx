module;
#include <wobjectdefs.h>
#include <QString>
#include <QDateTime>
#include <vector>
#include <mutex>

export module Diagnostics.Logger;

export namespace ArtifactCore {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

struct LogMessage {
    QDateTime timestamp;
    LogLevel level;
    QString message;
    QString context;
};

class Logger : public QObject {
    W_OBJECT(Logger)
public:
    static Logger* instance();

    void install();
    void uninstall();

    std::vector<LogMessage> getLogs();
    void clearLogs();

    void appendLog(LogLevel level, const QString& message, const QString& context = "");

    void logAdded(int level, const QString& message, const QString& context, const QDateTime& timestamp)
    W_SIGNAL(logAdded, level, message, context, timestamp)

    void logsCleared()
    W_SIGNAL(logsCleared)

private:
    Logger(QObject* parent = nullptr);

public:
    ~Logger() override;

    mutable std::mutex mutex_;
    std::vector<LogMessage> logs_;
    bool installed_ = false;
};

}
