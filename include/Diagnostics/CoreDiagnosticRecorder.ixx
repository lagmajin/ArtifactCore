module;
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

export module Core.Diagnostics.Recorder;

import Core.Diagnostics.Snapshot;
import Utils.Result;

export namespace ArtifactCore {

class DiagnosticRecorder {
public:
  static DiagnosticRecorder& instance() noexcept
  {
    static DiagnosticRecorder recorder;
    return recorder;
  }

  void record(DiagnosticEvent event)
  {
    if (!enabled_.load(std::memory_order_relaxed)) return;
    if (event.timestampNs == 0) {
      event.timestampNs = nowNs();
    }
    if (event.threadId == 0) {
      event.threadId = currentThreadId();
    }
    if (event.traceId == 0) {
      event.traceId = allocateTraceId();
    }
    event.sequence = nextSequence_.fetch_add(1, std::memory_order_relaxed) + 1;

    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(std::move(event));
    if (events_.size() > maxEvents_) {
      events_.erase(events_.begin(), events_.begin() +
          static_cast<std::ptrdiff_t>(events_.size() - maxEvents_));
    }
  }

  void setEnabled(bool enabled) noexcept
  {
    enabled_.store(enabled, std::memory_order_relaxed);
  }

  bool isEnabled() const noexcept
  {
    return enabled_.load(std::memory_order_relaxed);
  }

  void record(CoreDiagnosticSeverity severity,
              std::string code,
              std::string message,
              std::string component,
              std::string operation,
              std::string objectId = {},
              SourceLocation location = {})
  {
    record(makeDiagnosticEvent(severity, std::move(code), std::move(message),
                               std::move(component), std::move(operation),
                               std::move(objectId), location));
  }

  void recordError(const ErrorContext& error,
                   std::string component,
                   std::int64_t frameIndex = -1)
  {
    ErrorContext context = error;
    if (context.code == ErrorCode::None) context.code = ErrorCode::Failed;
    DiagnosticEvent event = makeDiagnosticEvent(context, std::move(component));
    event.frameIndex = frameIndex;
    record(std::move(event));
  }

  void recordStatus(const Status& status,
                    std::string component,
                    std::int64_t frameIndex = -1)
  {
    if (status.success) return;
    ErrorContext context = status.context;
    if (context.code == ErrorCode::None) {
      context.code = status.error == ErrorCode::None ? ErrorCode::Failed : status.error;
    }
    recordError(context, std::move(component), frameIndex);
  }

  template <typename T>
  bool recordResult(const Result<T>& result,
                   std::string component,
                   std::int64_t frameIndex = -1)
  {
    if (result.success()) return false;
    recordError(result.errorContext(), std::move(component), frameIndex);
    return true;
  }

  std::vector<DiagnosticEvent> snapshot() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
  }

  std::vector<DiagnosticEvent> eventsFor(
      const std::string& component,
      const std::string& objectId = {}) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiagnosticEvent> result;
    for (const auto& event : events_) {
      if (event.component != component) continue;
      if (!objectId.empty() && event.objectId != objectId) continue;
      result.push_back(event);
    }
    return result;
  }

  std::vector<DiagnosticEvent> since(std::uint64_t sequence) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiagnosticEvent> result;
    for (const auto& event : events_) {
      if (event.sequence > sequence) result.push_back(event);
    }
    return result;
  }

  DiagnosticSnapshot snapshotSince(std::uint64_t sequence,
                                   std::string component = {},
                                   std::string objectId = {},
                                   std::int64_t frameIndex = -1) const
  {
    DiagnosticSnapshot result;
    result.component = std::move(component);
    result.objectId = std::move(objectId);
    result.frameIndex = frameIndex;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      result.eventsTruncated = !events_.empty() &&
                               sequence + 1 < events_.front().sequence;
      for (const auto& event : events_) {
        if (event.sequence <= sequence) continue;
        if (!result.component.empty() && event.component != result.component) continue;
        if (!result.objectId.empty() && event.objectId != result.objectId) continue;
        result.recentEvents.push_back(event);
      }
    }
    result.refreshSequenceBounds();
    if (!result.recentEvents.empty()) {
      const auto& latestEvent = result.recentEvents.back();
      result.lastOperation = latestEvent.operation;
      result.threadId = latestEvent.threadId;
      switch (latestEvent.severity) {
      case CoreDiagnosticSeverity::Info: result.state = "ok"; break;
      case CoreDiagnosticSeverity::Warning: result.state = "warning"; break;
      case CoreDiagnosticSeverity::Error: result.state = "error"; break;
      case CoreDiagnosticSeverity::Fatal: result.state = "fatal"; break;
      }
    }
    return result;
  }

  std::vector<DiagnosticEvent> errorsFor(
      const std::string& component = {},
      const std::string& objectId = {}) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiagnosticEvent> result;
    for (const auto& event : events_) {
      const bool isError = event.severity == CoreDiagnosticSeverity::Error ||
                           event.severity == CoreDiagnosticSeverity::Fatal;
      if (!isError) continue;
      if (!component.empty() && event.component != component) continue;
      if (!objectId.empty() && event.objectId != objectId) continue;
      result.push_back(event);
    }
    return result;
  }

  std::optional<DiagnosticEvent> latest() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.empty()) return std::nullopt;
    return events_.back();
  }

  std::uint64_t latestSequence() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.empty() ? 0 : events_.back().sequence;
  }

  std::uint64_t oldestSequence() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.empty() ? 0 : events_.front().sequence;
  }

  bool missedSince(std::uint64_t sequence) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return !events_.empty() && sequence + 1 < events_.front().sequence;
  }

  std::vector<DiagnosticEvent> drain()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiagnosticEvent> result;
    result.swap(events_);
    return result;
  }

  DiagnosticSnapshot drainSnapshot(std::string component,
                                   std::string objectId = {},
                                   std::string state = {},
                                   std::string lastOperation = {},
                                   std::int64_t frameIndex = -1)
  {
    DiagnosticSnapshot result;
    result.component = std::move(component);
    result.objectId = std::move(objectId);
    result.state = std::move(state);
    result.lastOperation = std::move(lastOperation);
    result.frameIndex = frameIndex;
    result.recentEvents = drain();
    result.refreshSequenceBounds();
    return result;
  }

  std::string drainJson(std::string component,
                        std::string objectId = {},
                        std::string state = {},
                        std::string lastOperation = {},
                        std::int64_t frameIndex = -1)
  {
    return diagnosticSnapshotToJson(drainSnapshot(
        std::move(component),
        std::move(objectId),
        std::move(state),
        std::move(lastOperation),
        frameIndex));
  }

  DiagnosticSnapshot snapshot(std::string component,
                              std::string objectId = {},
                              std::string state = {},
                              std::string lastOperation = {},
                              std::int64_t frameIndex = -1) const
  {
    DiagnosticSnapshot result;
    result.component = std::move(component);
    result.objectId = std::move(objectId);
    result.state = std::move(state);
    result.lastOperation = std::move(lastOperation);
    result.frameIndex = frameIndex;
    result.recentEvents = snapshot();
    result.refreshSequenceBounds();
    return result;
  }

  DiagnosticSnapshot snapshotFor(std::string component,
                                 std::string objectId = {},
                                 std::int64_t frameIndex = -1) const
  {
    DiagnosticSnapshot result;
    result.component = component;
    result.objectId = objectId;
    result.frameIndex = frameIndex;
    result.recentEvents = eventsFor(component, objectId);
    result.refreshSequenceBounds();
    if (!result.recentEvents.empty()) {
      const auto& latestEvent = result.recentEvents.back();
      result.lastOperation = latestEvent.operation;
      result.threadId = latestEvent.threadId;
      switch (latestEvent.severity) {
      case CoreDiagnosticSeverity::Info: result.state = "ok"; break;
      case CoreDiagnosticSeverity::Warning: result.state = "warning"; break;
      case CoreDiagnosticSeverity::Error: result.state = "error"; break;
      case CoreDiagnosticSeverity::Fatal: result.state = "fatal"; break;
      }
    }
    return result;
  }

  std::string snapshotForJson(std::string component,
                              std::string objectId = {},
                              std::int64_t frameIndex = -1) const
  {
    return diagnosticSnapshotToJson(snapshotFor(
        std::move(component), std::move(objectId), frameIndex));
  }

  std::string snapshotJson(std::string component,
                           std::string objectId = {},
                           std::string state = {},
                           std::string lastOperation = {},
                           std::int64_t frameIndex = -1) const
  {
    return diagnosticSnapshotToJson(snapshot(
        std::move(component),
        std::move(objectId),
        std::move(state),
        std::move(lastOperation),
        frameIndex));
  }

  void clear()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
  }

  void setMaxEvents(std::size_t maxEvents)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    maxEvents_ = std::max<std::size_t>(1, maxEvents);
    if (events_.size() > maxEvents_) {
      events_.erase(events_.begin(), events_.begin() +
          static_cast<std::ptrdiff_t>(events_.size() - maxEvents_));
    }
  }

  std::size_t size() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
  }

  std::uint64_t allocateTraceId() noexcept
  {
    return nextTraceId_.fetch_add(1, std::memory_order_relaxed) + 1;
  }

private:
  static std::uint64_t nowNs() noexcept
  {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  }

  static std::uint64_t currentThreadId() noexcept
  {
    return static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
  }

  mutable std::mutex mutex_;
  std::atomic_bool enabled_{true};
  std::atomic_uint64_t nextSequence_{0};
  std::atomic_uint64_t nextTraceId_{0};
  std::vector<DiagnosticEvent> events_;
  std::size_t maxEvents_ = 256;
};

class DiagnosticScope {
public:
  DiagnosticScope(std::string component,
                  std::string operation,
                  std::string objectId = {},
                  SourceLocation location = {})
      : component_(std::move(component)),
        operation_(std::move(operation)),
        objectId_(std::move(objectId)),
        location_(location),
        startedNs_(nowNs()),
        traceId_(DiagnosticRecorder::instance().allocateTraceId())
  {
    if (!DiagnosticRecorder::instance().isEnabled()) {
      finished_ = true;
    }
  }

  DiagnosticScope(const DiagnosticScope&) = delete;
  DiagnosticScope& operator=(const DiagnosticScope&) = delete;

  ~DiagnosticScope()
  {
    if (!finished_) {
      finish(false, "scope exited without explicit completion");
    }
  }

  void finish(bool success = true, std::string message = {})
  {
    if (finished_) return;
    finished_ = true;
    const auto elapsedNs = nowNs() - startedNs_;
    if (message.empty()) {
      message = success ? "completed" : "failed";
    }
    auto event = makeDiagnosticEvent(
        success ? CoreDiagnosticSeverity::Info : CoreDiagnosticSeverity::Error,
        success ? "scope.completed" : "scope.failed",
        std::move(message),
        component_,
        operation_,
        objectId_,
        location_);
    event.timestampNs = nowNs();
    event.durationNs = elapsedNs;
    event.traceId = traceId_;
    DiagnosticRecorder::instance().record(std::move(event));
  }

  void finish(const ErrorContext& error)
  {
    if (finished_) return;
    finished_ = true;
    auto event = makeDiagnosticEvent(error, component_);
    if (!operation_.empty()) event.operation = operation_;
    event.objectId = objectId_.empty() ? error.objectId : objectId_;
    event.location = location_.file && location_.file[0] != '\0'
        ? location_
        : error.location;
    event.timestampNs = nowNs();
    event.durationNs = event.timestampNs - startedNs_;
    event.traceId = traceId_;
    DiagnosticRecorder::instance().record(std::move(event));
  }

private:
  static std::uint64_t nowNs() noexcept
  {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  }

  std::string component_;
  std::string operation_;
  std::string objectId_;
  SourceLocation location_;
  std::uint64_t startedNs_ = 0;
  std::uint64_t traceId_ = 0;
  bool finished_ = false;
};

} // namespace ArtifactCore
