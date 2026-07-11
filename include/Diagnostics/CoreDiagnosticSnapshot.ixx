module;
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

export module Core.Diagnostics.Snapshot;

import Utils.Result;

export namespace ArtifactCore {

enum class CoreDiagnosticSeverity : std::uint8_t {
  Info,
  Warning,
  Error,
  Fatal
};

struct DiagnosticEvent {
  CoreDiagnosticSeverity severity = CoreDiagnosticSeverity::Info;
  std::string code;
  std::string message;
  std::string component;
  std::string operation;
  std::string objectId;
  SourceLocation location;
  std::uint64_t timestampNs = 0;
  std::uint64_t durationNs = 0;
  std::uint64_t sequence = 0;
  std::uint64_t threadId = 0;
  std::uint64_t traceId = 0;
  std::int64_t frameIndex = -1;

  bool isFailure() const noexcept
  {
    return severity == CoreDiagnosticSeverity::Error ||
           severity == CoreDiagnosticSeverity::Fatal;
  }
};

struct DiagnosticSnapshot {
  std::uint32_t version = 1;
  std::string component;
  std::string objectId;
  std::string state;
  std::string lastOperation;
  std::int64_t frameIndex = -1;
  std::uint64_t threadId = 0;
  bool eventsTruncated = false;
  std::uint64_t firstSequence = 0;
  std::uint64_t lastSequence = 0;
  std::vector<DiagnosticEvent> recentEvents;

  void refreshSequenceBounds() noexcept
  {
    firstSequence = recentEvents.empty() ? 0 : recentEvents.front().sequence;
    lastSequence = recentEvents.empty() ? 0 : recentEvents.back().sequence;
  }

  void normalizeOrder()
  {
    std::stable_sort(recentEvents.begin(), recentEvents.end(),
        [](const DiagnosticEvent& left, const DiagnosticEvent& right) {
          const bool leftHasSequence = left.sequence != 0;
          const bool rightHasSequence = right.sequence != 0;
          if (leftHasSequence != rightHasSequence) {
            return leftHasSequence;
          }
          if (leftHasSequence) return left.sequence < right.sequence;
          return left.timestampNs < right.timestampNs;
        });
    refreshSequenceBounds();
  }

  std::size_t count(CoreDiagnosticSeverity severity) const noexcept
  {
    std::size_t result = 0;
    for (const auto& event : recentEvents) {
      if (event.severity == severity) ++result;
    }
    return result;
  }

  bool hasErrors() const noexcept
  {
    return count(CoreDiagnosticSeverity::Error) != 0 ||
           count(CoreDiagnosticSeverity::Fatal) != 0;
  }

  const DiagnosticEvent* latestFailure() const noexcept
  {
    const DiagnosticEvent* latest = nullptr;
    for (const auto& event : recentEvents) {
      if (!event.isFailure()) continue;
      if (!latest) {
        latest = &event;
        continue;
      }
      const bool eventHasSequence = event.sequence != 0;
      const bool latestHasSequence = latest->sequence != 0;
      if (eventHasSequence != latestHasSequence) {
        if (eventHasSequence) latest = &event;
      } else if ((eventHasSequence && event.sequence > latest->sequence) ||
                 (!eventHasSequence && event.timestampNs > latest->timestampNs)) {
        latest = &event;
      }
    }
    return latest;
  }

  void merge(const DiagnosticSnapshot& other, std::size_t maxEvents = 128)
  {
    if (component.empty()) component = other.component;
    if (objectId.empty()) objectId = other.objectId;
    if (state.empty()) state = other.state;
    if (lastOperation.empty()) lastOperation = other.lastOperation;
    if (frameIndex < 0) frameIndex = other.frameIndex;
    if (threadId == 0) threadId = other.threadId;
    eventsTruncated = eventsTruncated || other.eventsTruncated;
    recentEvents.insert(recentEvents.end(),
                        other.recentEvents.begin(),
                        other.recentEvents.end());
    if (recentEvents.size() > maxEvents) {
      eventsTruncated = true;
      const auto first = recentEvents.size() - maxEvents;
      recentEvents.erase(recentEvents.begin(), recentEvents.begin() + first);
    }
    normalizeOrder();
  }

  void addEvent(DiagnosticEvent event, std::size_t maxEvents = 64)
  {
    recentEvents.push_back(std::move(event));
    if (maxEvents == 0) {
      eventsTruncated = true;
      recentEvents.clear();
      refreshSequenceBounds();
      return;
    }
    if (recentEvents.size() > maxEvents) {
      eventsTruncated = true;
      const auto first = recentEvents.size() - maxEvents;
      recentEvents.erase(recentEvents.begin(), recentEvents.begin() + first);
    }
    normalizeOrder();
  }
};

inline DiagnosticEvent makeDiagnosticEvent(
    CoreDiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string component,
    std::string operation,
    std::string objectId = {},
    SourceLocation location = {})
{
  return DiagnosticEvent{
      severity,
      std::move(code),
      std::move(message),
      std::move(component),
      std::move(operation),
      std::move(objectId),
      location};
}

inline DiagnosticEvent makeDiagnosticEvent(
    const ErrorContext& error,
    std::string component,
    CoreDiagnosticSeverity severity = CoreDiagnosticSeverity::Error)
{
  DiagnosticEvent event = makeDiagnosticEvent(
      severity,
      std::string("core.") + errorCodeName(error.code),
      error.message.empty() ? std::string(errorCodeName(error.code)) : error.message,
      std::move(component),
      error.operation,
      error.objectId,
      error.location);
  event.traceId = error.traceId;
  return event;
}

inline const char* diagnosticSeverityName(CoreDiagnosticSeverity severity) noexcept
{
  switch (severity) {
  case CoreDiagnosticSeverity::Info: return "info";
  case CoreDiagnosticSeverity::Warning: return "warning";
  case CoreDiagnosticSeverity::Error: return "error";
  case CoreDiagnosticSeverity::Fatal: return "fatal";
  }
  return "info";
}

inline void appendJsonString(std::string& output, const std::string& value)
{
  output.push_back('"');
  for (const char character : value) {
    switch (character) {
    case '\\': output += "\\\\"; break;
    case '"': output += "\\\""; break;
    case '\b': output += "\\b"; break;
    case '\f': output += "\\f"; break;
    case '\n': output += "\\n"; break;
    case '\r': output += "\\r"; break;
    case '\t': output += "\\t"; break;
    default:
      if (static_cast<unsigned char>(character) < 0x20) {
        static constexpr char hex[] = "0123456789abcdef";
        output += "\\u00";
        output.push_back(hex[(static_cast<unsigned char>(character) >> 4) & 0x0f]);
        output.push_back(hex[static_cast<unsigned char>(character) & 0x0f]);
      } else {
        output.push_back(character);
      }
      break;
    }
  }
  output.push_back('"');
}

inline std::string diagnosticSnapshotToJson(const DiagnosticSnapshot& snapshot)
{
  std::string output = "{\"version\":" + std::to_string(snapshot.version);
  output += ",\"component\":";
  appendJsonString(output, snapshot.component);
  output += ",\"objectId\":";
  appendJsonString(output, snapshot.objectId);
  output += ",\"state\":";
  appendJsonString(output, snapshot.state);
  output += ",\"lastOperation\":";
  appendJsonString(output, snapshot.lastOperation);
  output += ",\"frameIndex\":" + std::to_string(snapshot.frameIndex);
  output += ",\"threadId\":" + std::to_string(snapshot.threadId);
  output += ",\"eventsTruncated\":" +
            std::string(snapshot.eventsTruncated ? "true" : "false");
  output += ",\"firstSequence\":" + std::to_string(snapshot.firstSequence);
  output += ",\"lastSequence\":" + std::to_string(snapshot.lastSequence);
  output += ",\"infoCount\":" +
            std::to_string(snapshot.count(CoreDiagnosticSeverity::Info));
  output += ",\"warningCount\":" +
            std::to_string(snapshot.count(CoreDiagnosticSeverity::Warning));
  output += ",\"errorCount\":" +
            std::to_string(snapshot.count(CoreDiagnosticSeverity::Error));
  output += ",\"fatalCount\":" +
            std::to_string(snapshot.count(CoreDiagnosticSeverity::Fatal));
  output += ",\"hasErrors\":" +
            std::string(snapshot.hasErrors() ? "true" : "false");
  const auto* latestFailure = snapshot.latestFailure();
  output += ",\"latestFailureCode\":";
  appendJsonString(output, latestFailure ? latestFailure->code : std::string());
  output += ",\"latestFailureMessage\":";
  appendJsonString(output, latestFailure ? latestFailure->message : std::string());
  output += ",\"latestFailureOperation\":";
  appendJsonString(output, latestFailure ? latestFailure->operation : std::string());
  output += ",\"latestFailureObjectId\":";
  appendJsonString(output, latestFailure ? latestFailure->objectId : std::string());
  output += ",\"latestFailureFile\":";
  appendJsonString(output,
                   latestFailure && latestFailure->location.file
                       ? latestFailure->location.file : "");
  output += ",\"latestFailureFunction\":";
  appendJsonString(output,
                   latestFailure && latestFailure->location.function
                       ? latestFailure->location.function : "");
  output += ",\"latestFailureLine\":" +
            std::to_string(latestFailure ? latestFailure->location.line : 0);
  output += ",\"latestFailureSequence\":" +
            std::to_string(latestFailure ? latestFailure->sequence : 0);
  output += ",\"latestFailureThreadId\":" +
            std::to_string(latestFailure ? latestFailure->threadId : 0);
  output += ",\"latestFailureTraceId\":" +
            std::to_string(latestFailure ? latestFailure->traceId : 0);
  output += ",\"recentEvents\":[";
  for (std::size_t index = 0; index < snapshot.recentEvents.size(); ++index) {
    if (index != 0) output.push_back(',');
    const auto& event = snapshot.recentEvents[index];
    output += "{\"severity\":";
    appendJsonString(output, diagnosticSeverityName(event.severity));
    output += ",\"code\":";
    appendJsonString(output, event.code);
    output += ",\"message\":";
    appendJsonString(output, event.message);
    output += ",\"component\":";
    appendJsonString(output, event.component);
    output += ",\"operation\":";
    appendJsonString(output, event.operation);
    output += ",\"objectId\":";
    appendJsonString(output, event.objectId);
    output += ",\"file\":";
    appendJsonString(output, event.location.file ? event.location.file : "");
    output += ",\"function\":";
    appendJsonString(output, event.location.function ? event.location.function : "");
    output += ",\"line\":" + std::to_string(event.location.line);
    output += ",\"timestampNs\":" + std::to_string(event.timestampNs);
    output += ",\"durationNs\":" + std::to_string(event.durationNs);
    output += ",\"sequence\":" + std::to_string(event.sequence);
    output += ",\"threadId\":" + std::to_string(event.threadId);
    output += ",\"traceId\":" + std::to_string(event.traceId);
    output += ",\"frameIndex\":" + std::to_string(event.frameIndex) + '}';
  }
  output += "]}";
  return output;
}

} // namespace ArtifactCore
