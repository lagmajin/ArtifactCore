module;
#include <cstdint>
#include <string>
#include <typeinfo>
#include <vector>

export module Container.Debug.Text;

import Container.Debug;
import Container.NamedList;

export namespace ArtifactCore {

inline std::string toDebugText(const ContainerElementSample& sample);

inline std::string toDebugText(const ContainerDebugInfo& info)
{
  std::string text;
  text += info.name.value;
  text += " [";
  text += toString(info.domain);
  text += "] ";
  text += info.valueType;
  text += " count=";
  text += std::to_string(info.count);
  text += " capacity=";
  text += std::to_string(info.capacity);
  text += " bytes=";
  text += std::to_string(info.approximateBytes);
  if (info.owner.name && info.owner.name[0] != '\0') {
    text += " owner=";
    text += info.owner.name;
  }
  return text;
}

inline std::string toDebugText(const ContainerDebugSnapshot& snapshot)
{
  std::string text = toDebugText(snapshot.info);
  text += " version=";
  text += std::to_string(snapshot.counters.version);
  text += " mutations=";
  text += std::to_string(snapshot.counters.mutationCount);
  text += " reads=";
  text += std::to_string(snapshot.counters.readCount);
  text += " failed=";
  text += std::to_string(snapshot.counters.failedAccessCount);
  text += " samples=";
  text += std::to_string(snapshot.samples.size());
  if (snapshot.createdAt.file && snapshot.createdAt.file[0] != '\0') {
    text += " createdAt=";
    text += snapshot.createdAt.file;
    text += ":";
    text += std::to_string(snapshot.createdAt.line);
  }
  if (snapshot.lastMutatedAt.file && snapshot.lastMutatedAt.file[0] != '\0') {
    text += " mutatedAt=";
    text += snapshot.lastMutatedAt.file;
    text += ":";
    text += std::to_string(snapshot.lastMutatedAt.line);
  }
  if (snapshot.lastFailedAccessAt.file && snapshot.lastFailedAccessAt.file[0] != '\0') {
    text += " failedAt=";
    text += snapshot.lastFailedAccessAt.file;
    text += ":";
    text += std::to_string(snapshot.lastFailedAccessAt.line);
  }
  if (snapshot.lastMutation.operation && snapshot.lastMutation.operation[0] != '\0') {
    text += " lastMutation=";
    text += snapshot.lastMutation.operation;
  }
  if (snapshot.hasSamples()) {
    text += " sample0=";
    if (!snapshot.samples.empty()) {
      text += toDebugText(snapshot.samples.front());
    }
  }
  return text;
}

inline std::string toDebugText(const ContainerWatchHit& hit)
{
  std::string text;
  text += hit.reason ? hit.reason : "";
  text += " @";
  text += hit.location.file ? hit.location.file : "";
  text += ":";
  text += std::to_string(hit.location.line);
  text += " -> ";
  text += toDebugText(hit.snapshot);
  return text;
}

template <typename T>
inline std::string toDebugText(const NamedList<T>& values)
{
  return toDebugText(values.debugSnapshot());
}

template <typename T>
inline std::string toDebugText(const std::vector<T>& values)
{
  std::string text = "std::vector[size=" + std::to_string(values.size()) + "]";
  return text;
}

inline std::string toDebugText(const ContainerElementSample& sample)
{
  std::string text;
  text += "sample[";
  text += std::to_string(sample.index);
  text += "]@";
  text += std::to_string(reinterpret_cast<std::uintptr_t>(sample.address));
  if (sample.note && sample.note[0] != '\0') {
    text += " ";
    text += sample.note;
  }
  return text;
}

}
