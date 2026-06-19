module;
#include <cstddef>
#include <string>

export module Container.Debug;

import Container.NamedVector;

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
};

struct ContainerMutationRecord {
  const char* operation = "";
  ContainerSourceLocation location;
  std::size_t version = 0;
  std::size_t countBefore = 0;
  std::size_t countAfter = 0;
  const char* note = "";
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
  NamedVector<ContainerElementSample> samples{ContainerName{"ContainerDebugSamples"}};

  constexpr bool isEmpty() const noexcept
  {
    return info.isEmpty();
  }

  constexpr bool hasSamples() const noexcept
  {
    return !samples.empty();
  }
};

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

inline constexpr const char* toString(const ContainerWatchRule& rule) noexcept
{
  return rule.watchMutation
      ? "watchMutation"
      : (rule.watchFailedAccess ? "watchFailedAccess" : "watch");
}

}
