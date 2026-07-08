module;
#include <cstdint>
#include <string>
#include <typeinfo>
#include <vector>

export module Container.Debug.Text;

import Core.ArtifactString;
import Container.Debug;
import Container.NamedList;
import Container.SmallVector;
import Container.NameMap;
import Container.IdMap;

export namespace ArtifactCore {

inline ZeroString toDebugTextZero(const ContainerElementSample& sample);
inline ZeroString toDebugTextZero(const ContainerDebugInfo& info);
inline ZeroString toDebugTextZero(const ContainerDebugSnapshot& snapshot);
inline ZeroString toDebugTextZero(const ContainerWatchHit& hit);
template <typename T, std::size_t N>
inline ZeroString toDebugTextZero(const SmallVector<T, N>& values);
template <typename K, typename V>
inline ZeroString toDebugTextZero(const NameMap<K, V>& values);
template <typename K, typename V>
inline ZeroString toDebugTextZero(const IdMap<K, V>& values);
template <typename T>
inline ZeroString toDebugTextZero(const NamedList<T>& values);
template <typename T>
inline ZeroString toDebugTextZero(const std::vector<T>& values);

inline std::string toDebugText(const ContainerElementSample& sample) {
  return std::string(toDebugTextZero(sample));
}

inline std::string toDebugText(const ContainerDebugInfo& info) {
  return std::string(toDebugTextZero(info));
}

inline std::string toDebugText(const ContainerDebugSnapshot& snapshot) {
  return std::string(toDebugTextZero(snapshot));
}

inline std::string toDebugText(const ContainerWatchHit& hit) {
  return std::string(toDebugTextZero(hit));
}

template <typename T, std::size_t N>
inline std::string toDebugText(const SmallVector<T, N>& values) {
  return std::string(toDebugTextZero(values));
}

template <typename K, typename V>
inline std::string toDebugText(const NameMap<K, V>& values) {
  return std::string(toDebugTextZero(values));
}

template <typename K, typename V>
inline std::string toDebugText(const IdMap<K, V>& values) {
  return std::string(toDebugTextZero(values));
}

template <typename T>
inline std::string toDebugText(const NamedList<T>& values) {
  return std::string(toDebugTextZero(values));
}

template <typename T>
inline std::string toDebugText(const std::vector<T>& values) {
  return std::string(toDebugTextZero(values));
}

inline ZeroString toDebugTextZero(const ContainerElementSample& sample) {
  ZeroString text;
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

inline ZeroString toDebugTextZero(const ContainerDebugInfo& info) {
  ZeroString text;
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

inline ZeroString toDebugTextZero(const ContainerDebugSnapshot& snapshot) {
  ZeroString text = toDebugTextZero(snapshot.info);
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
  if (snapshot.hasSamples() && !snapshot.samples.empty()) {
    text += " sample0=";
    text += toDebugTextZero(snapshot.samples.front());
  }
  return text;
}

inline ZeroString toDebugTextZero(const ContainerWatchHit& hit) {
  ZeroString text;
  text += hit.reason ? hit.reason : "";
  text += " @";
  text += hit.location.file ? hit.location.file : "";
  text += ":";
  text += std::to_string(hit.location.line);
  text += " -> ";
  text += toDebugTextZero(hit.snapshot);
  return text;
}

template <typename T, std::size_t N>
inline ZeroString toDebugTextZero(const SmallVector<T, N>& values) {
  return toDebugTextZero(values.debugSnapshot());
}

template <typename K, typename V>
inline ZeroString toDebugTextZero(const NameMap<K, V>& values) {
  return toDebugTextZero(values.debugSnapshot());
}

template <typename K, typename V>
inline ZeroString toDebugTextZero(const IdMap<K, V>& values) {
  return toDebugTextZero(values.debugSnapshot());
}

template <typename T>
inline ZeroString toDebugTextZero(const NamedList<T>& values) {
  return toDebugTextZero(values.debugSnapshot());
}

template <typename T>
inline ZeroString toDebugTextZero(const std::vector<T>& values) {
  ZeroString text = "std::vector[size=";
  text += std::to_string(values.size());
  text += "]";
  return text;
}

}
