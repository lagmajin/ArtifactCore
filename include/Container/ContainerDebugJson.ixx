module;
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

export module Container.Debug.Json;

import Container.Debug;
import Container.Debug.Registry;

export namespace ArtifactCore {

inline QJsonObject toJson(const ContainerSourceLocation& location)
{
  QJsonObject json;
  json.insert(QStringLiteral("file"), QString::fromUtf8(location.file ? location.file : ""));
  json.insert(QStringLiteral("function"), QString::fromUtf8(location.function ? location.function : ""));
  json.insert(QStringLiteral("line"), location.line);
  return json;
}

inline QJsonObject toJson(const ContainerOwner& owner)
{
  QJsonObject json;
  json.insert(QStringLiteral("name"), QString::fromUtf8(owner.name ? owner.name : ""));
  json.insert(QStringLiteral("id"), QString::fromUtf8(owner.id ? owner.id : ""));
  return json;
}

inline QJsonObject toJson(const ContainerDebugCounters& counters)
{
  QJsonObject json;
  json.insert(QStringLiteral("version"), static_cast<double>(counters.version));
  json.insert(QStringLiteral("mutationCount"), static_cast<double>(counters.mutationCount));
  json.insert(QStringLiteral("readCount"), static_cast<double>(counters.readCount));
  json.insert(QStringLiteral("failedAccessCount"), static_cast<double>(counters.failedAccessCount));
  json.insert(QStringLiteral("maxCountSeen"), static_cast<double>(counters.maxCountSeen));
  json.insert(QStringLiteral("addedCount"), static_cast<double>(counters.addedCount));
  json.insert(QStringLiteral("removedCount"), static_cast<double>(counters.removedCount));
  json.insert(QStringLiteral("capacityChangeCount"), static_cast<double>(counters.capacityChangeCount));
  json.insert(QStringLiteral("maxCapacitySeen"), static_cast<double>(counters.maxCapacitySeen));
  json.insert(QStringLiteral("maxApproximateBytesSeen"), static_cast<double>(counters.maxApproximateBytesSeen));
  return json;
}

inline QJsonObject toJson(const ContainerMutationRecord& mutation)
{
  QJsonObject json;
  json.insert(QStringLiteral("operation"), QString::fromUtf8(mutation.operation ? mutation.operation : ""));
  json.insert(QStringLiteral("location"), toJson(mutation.location));
  json.insert(QStringLiteral("version"), static_cast<double>(mutation.version));
  json.insert(QStringLiteral("countBefore"), static_cast<double>(mutation.countBefore));
  json.insert(QStringLiteral("countAfter"), static_cast<double>(mutation.countAfter));
  json.insert(QStringLiteral("note"), QString::fromUtf8(mutation.note ? mutation.note : ""));
  return json;
}

inline QJsonObject toJson(const ContainerDebugNote& note)
{
  QJsonObject json;
  json.insert(QStringLiteral("timestampMilliseconds"), static_cast<double>(note.timestampMilliseconds));
  json.insert(QStringLiteral("severity"), QString::fromUtf8(toString(note.severity)));
  json.insert(QStringLiteral("author"), QString::fromUtf8(toString(note.author)));
  json.insert(QStringLiteral("text"), QString::fromStdString(note.text));
  json.insert(QStringLiteral("location"), toJson(note.location));
  json.insert(QStringLiteral("observedVersion"), static_cast<double>(note.observedVersion));
  return json;
}

inline QJsonObject toJson(const ContainerElementSample& sample)
{
  QJsonObject json;
  json.insert(QStringLiteral("index"), static_cast<double>(sample.index));
  json.insert(QStringLiteral("address"), QString::number(reinterpret_cast<quintptr>(sample.address)));
  json.insert(QStringLiteral("note"), QString::fromUtf8(sample.note ? sample.note : ""));
  return json;
}

inline QJsonObject toJson(const ContainerDebugInfo& info)
{
  QJsonObject json;
  json.insert(QStringLiteral("name"), QString::fromUtf8(info.name.value ? info.name.value : ""));
  json.insert(QStringLiteral("domain"), QString::fromUtf8(toString(info.domain)));
  json.insert(QStringLiteral("owner"), toJson(info.owner));
  json.insert(QStringLiteral("valueType"), QString::fromUtf8(info.valueType ? info.valueType : ""));
  json.insert(QStringLiteral("count"), static_cast<double>(info.count));
  json.insert(QStringLiteral("capacity"), static_cast<double>(info.capacity));
  json.insert(QStringLiteral("approximateBytes"), static_cast<double>(info.approximateBytes));
  return json;
}

inline QJsonObject toJson(const ContainerDebugSnapshot& snapshot)
{
  QJsonObject json;
  json.insert(QStringLiteral("info"), toJson(snapshot.info));
  json.insert(QStringLiteral("counters"), toJson(snapshot.counters));
  json.insert(QStringLiteral("createdAt"), toJson(snapshot.createdAt));
  json.insert(QStringLiteral("lastMutatedAt"), toJson(snapshot.lastMutatedAt));
  json.insert(QStringLiteral("lastFailedAccessAt"), toJson(snapshot.lastFailedAccessAt));
  json.insert(QStringLiteral("lastMutation"), toJson(snapshot.lastMutation));
  QJsonArray samples;
  for (const auto& sample : snapshot.samples) {
    samples.append(toJson(sample));
  }
  json.insert(QStringLiteral("samples"), samples);
  QJsonArray notes;
  for (const auto& note : snapshot.notes) notes.append(toJson(note));
  json.insert(QStringLiteral("notes"), notes);
  return json;
}

inline QJsonArray toJson(const ContainerDebugRegistry& registry)
{
  QJsonArray entries;
  for (const auto& id : registry.registeredIds()) {
    ContainerDebugSnapshot snapshot;
    QJsonObject entry;
    entry.insert(QStringLiteral("id"), QString::fromStdString(id));
    const bool available = registry.inspect(id, snapshot);
    entry.insert(QStringLiteral("available"), available);
    if (available) entry.insert(QStringLiteral("snapshot"), toJson(snapshot));
    entries.append(entry);
  }
  return entries;
}

}
