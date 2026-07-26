module;
#include <utility>
#include <vector>
#include <array>
#include <atomic>
#include <cstdint>

#include <mutex>
#include <wobjectdefs.h>
#include <QFile>
#include <QString>
#include <QDateTime>

export module Diagnostics.Logger;

import Core.Diagnostics.Snapshot;
import Core.Diagnostics.Recorder;

export namespace ArtifactCore {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

enum class LogFileFormat {
    Text,
    JsonLines
};

enum class LogCategory : std::uint32_t {
    General = 0,
    App,
    Project,
    Timeline,
    RenderVP,
    RenderGPU,
    RenderPass,
    RenderResource,
    MediaDecode,
    NetworkFarm,
    ScriptRuntime,
    Diagnostics
};

struct FastLogRecord {
    std::uint64_t timestampTicks = 0;
    std::uint32_t category = 0;
    std::uint32_t threadId = 0;
    std::uint32_t frame = 0xffffffffu;
    std::uint16_t level = 0;
    std::uint16_t length = 0;
    std::array<char, 192> message{};
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
    void setFileLoggingEnabled(bool enabled);
    void setLogFilePath(const QString& path);
    QString logFilePath() const;
    void setMaxLogFileBytes(std::uint64_t bytes) noexcept;
    void setLogFileFormat(LogFileFormat format) noexcept;
    LogFileFormat logFileFormat() const noexcept;
    void flushFile();

    std::vector<LogMessage> getLogs() const;
    void clearLogs();

    void appendLog(LogLevel level, const QString& message, const QString& context = "");
    bool tryFastLog(LogLevel level, LogCategory category, const char* message,
                    std::uint32_t frame = 0xffffffffu) noexcept;
    bool tryFastLogFormat(LogLevel level, LogCategory category, std::uint32_t frame,
                          const char* format, ...) noexcept;
    void setCategoryEnabled(LogCategory category, bool enabled) noexcept;
    bool isCategoryEnabled(LogCategory category) const noexcept;
    std::size_t drainFastLogs(std::size_t maxRecords = 256);
    std::uint64_t droppedFastLogCount() const noexcept;
    void appendDiagnostic(const DiagnosticEvent& event);
    void appendDiagnostics(const DiagnosticSnapshot& snapshot);
    void flushDiagnostics(DiagnosticRecorder& recorder);

    void recordDiagnostic(LogLevel level,
                          const QString& message,
                          const QString& context = "");

    void logAdded(int level, const QString& message, const QString& context, const QDateTime& timestamp)
    W_SIGNAL(logAdded, level, message, context, timestamp)

    void logsCleared()
    W_SIGNAL(logsCleared)

private:
    bool ensureLogFileReady();
    void writeLineToLogFile(const QString& line);
    QString formatLogLine(const LogMessage& logMsg) const;

    Logger(QObject* parent = nullptr);

public:
    ~Logger() override;

    mutable std::mutex mutex_;
    std::vector<LogMessage> logs_;
    QString logFilePath_;
    QFile logFile_;
    bool installed_ = false;
    bool fileLoggingRequested_ = true;
    bool fileLoggingEnabled_ = false;
    std::uint64_t maxLogFileBytes_ = 10ull * 1024ull * 1024ull;
    LogFileFormat logFileFormat_ = LogFileFormat::Text;
    static constexpr std::size_t fastLogCapacity_ = 8192;
    std::array<FastLogRecord, fastLogCapacity_> fastLogBuffer_{};
    std::atomic<std::size_t> fastLogWrite_{0};
    std::atomic<std::size_t> fastLogRead_{0};
    std::atomic<std::uint64_t> fastLogDropped_{0};
    std::atomic_flag fastLogProducerLock_ = ATOMIC_FLAG_INIT;
    std::array<std::atomic_bool, 12> categoryEnabled_{};
};

}
