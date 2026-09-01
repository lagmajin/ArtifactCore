module;
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

export module Container.Debug;

export namespace ArtifactCore {

enum class ContainerDomain : unsigned char {
  Unknown,
  Timeline,
  Render,
  Selection,
  Asset,
  Cache,
  Diagnostics
};

struct ContainerName {
  const char* value = "";

  constexpr ContainerName() noexcept = default;
  constexpr explicit ContainerName(const char* name) noexcept
    : value(name ? name : "")
  {
  }

  constexpr bool isEmpty() const noexcept
  {
    return value == nullptr || value[0] == '\0';
  }
};

struct ContainerSourceLocation {
  const char* file = "";
  const char* function = "";
  int line = 0;
};

struct ContainerOwner {
  const char* name = "";
  const char* id = "";
};

struct ContainerDebugCounters {
  std::size_t version = 0;
  std::size_t mutationCount = 0;
  std::size_t readCount = 0;
  std::size_t failedAccessCount = 0;
  std::size_t maxCountSeen = 0;
  std::size_t addedCount = 0;
  std::size_t removedCount = 0;
  std::size_t capacityChangeCount = 0;
  std::size_t maxCapacitySeen = 0;
  std::size_t maxApproximateBytesSeen = 0;
};

struct ContainerDebugCheckpoint {
  std::size_t version = 0;
  std::size_t readCount = 0;
  std::size_t failedAccessCount = 0;
};

struct ContainerMutationRecord {
  const char* operation = "";
  ContainerSourceLocation location;
  std::size_t version = 0;
  std::size_t countBefore = 0;
  std::size_t countAfter = 0;
  const char* note = "";
};

enum class ContainerDebugNoteSeverity : unsigned char {
  Info,
  Warning,
  Error,
  Hypothesis
};

enum class ContainerDebugNoteAuthor : unsigned char {
  Runtime,
  Developer,
  AI
};

struct ContainerDebugNote {
  std::uint64_t timestampMilliseconds = 0;
  ContainerDebugNoteSeverity severity = ContainerDebugNoteSeverity::Info;
  ContainerDebugNoteAuthor author = ContainerDebugNoteAuthor::Runtime;
  std::string text;
  ContainerSourceLocation location;
  std::size_t observedVersion = 0;
};

template <typename T>
struct ContainerDebugValueCheckpoint {
  const void* source = nullptr;
  std::uint64_t capturedAtMilliseconds = 0;
  std::size_t version = 0;
  std::size_t failedAccessCount = 0;
  std::vector<T> values;
};

struct ContainerDebugVerification {
  bool sourceMatches = false;
  bool unchangedSinceCheckpoint = false;
  std::size_t expectedVersion = 0;
  std::size_t actualVersion = 0;
  std::size_t expectedCount = 0;
  std::size_t actualCount = 0;
  std::size_t expectedFailedAccessCount = 0;
  std::size_t actualFailedAccessCount = 0;
};

struct ContainerElementSample {
  std::size_t index = 0;
  const void* address = nullptr;
  const char* note = "";
};

struct ContainerWatchRule {
  std::size_t minCount = 0;
  std::size_t maxCount = 0;
  std::size_t minVersion = 0;
  std::size_t maxVersion = 0;
  std::size_t minReadCount = 0;
  std::size_t maxReadCount = 0;
  bool watchEmpty = false;
  bool watchFailedAccess = false;
  bool watchMutation = false;
};

struct ContainerDebugInfo {
  ContainerName name;
  ContainerDomain domain = ContainerDomain::Unknown;
  ContainerOwner owner;
  const char* valueType = "";
  std::size_t count = 0;
  std::size_t capacity = 0;
  std::size_t approximateBytes = 0;

  constexpr bool isEmpty() const noexcept
  {
    return count == 0;
  }

  constexpr bool hasName() const noexcept
  {
    return !name.isEmpty();
  }
};

struct ContainerDebugSnapshot {
  ContainerDebugInfo info;
  ContainerDebugCounters counters;
  ContainerSourceLocation createdAt;
  ContainerSourceLocation lastMutatedAt;
  ContainerSourceLocation lastFailedAccessAt;
  ContainerMutationRecord lastMutation;
  std::vector<ContainerElementSample> samples;
  std::vector<ContainerDebugNote> notes;

  constexpr bool isEmpty() const noexcept
  {
    return info.isEmpty();
  }

  constexpr bool hasSamples() const noexcept
  {
    return !samples.empty();
  }
};

inline std::uint64_t containerDebugNowMilliseconds() noexcept
{
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());
}

struct ContainerWatchHit {
  const char* reason = "";
  ContainerSourceLocation location;
  ContainerDebugSnapshot snapshot;
};

inline constexpr ContainerName unnamedContainer() noexcept
{
  return ContainerName{};
}

inline constexpr ContainerName namedContainer(const char* name) noexcept
{
  return ContainerName{name};
}

inline constexpr ContainerSourceLocation containerSourceLocation(
  const char* file,
  const char* function,
  int line) noexcept
{
  return ContainerSourceLocation{file, function, line};
}

inline constexpr ContainerOwner containerOwner(const char* name, const char* id = "") noexcept
{
  return ContainerOwner{name, id};
}

inline constexpr ContainerSourceLocation containerHere(
  const char* file,
  const char* function,
  int line) noexcept
{
  return containerSourceLocation(file, function, line);
}

#define ARTIFACT_CONTAINER_HERE ::ArtifactCore::containerHere(__FILE__, __func__, __LINE__)
#define ARTIFACT_CONTAINER_OWNER(name, id) ::ArtifactCore::containerOwner(name, id)

inline constexpr const char* toString(ContainerDomain domain) noexcept
{
  switch (domain) {
  case ContainerDomain::Unknown: return "Unknown";
  case ContainerDomain::Timeline: return "Timeline";
  case ContainerDomain::Render: return "Render";
  case ContainerDomain::Selection: return "Selection";
  case ContainerDomain::Asset: return "Asset";
  case ContainerDomain::Cache: return "Cache";
  case ContainerDomain::Diagnostics: return "Diagnostics";
  }
  return "Unknown";
}

inline constexpr const char* toString(ContainerDebugNoteSeverity severity) noexcept
{
  switch (severity) {
  case ContainerDebugNoteSeverity::Info: return "info";
  case ContainerDebugNoteSeverity::Warning: return "warning";
  case ContainerDebugNoteSeverity::Error: return "error";
  case ContainerDebugNoteSeverity::Hypothesis: return "hypothesis";
  }
  return "info";
}

inline constexpr const char* toString(ContainerDebugNoteAuthor author) noexcept
{
  switch (author) {
  case ContainerDebugNoteAuthor::Runtime: return "runtime";
  case ContainerDebugNoteAuthor::Developer: return "developer";
  case ContainerDebugNoteAuthor::AI: return "ai";
  }
  return "runtime";
}

inline constexpr const char* toString(const ContainerWatchRule& rule) noexcept
{
  return rule.watchMutation
      ? "watchMutation"
      : (rule.watchFailedAccess ? "watchFailedAccess" : "watch");
}

}
