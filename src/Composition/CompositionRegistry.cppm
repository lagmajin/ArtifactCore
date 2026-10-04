module;
#include <QString>
#include <QStringList>

module Composition.Registry;

import Container.NameMap;

namespace ArtifactCore {

namespace {
/// Single process-wide store. A function-local static keeps the registry
/// allocation-free to set up and avoids a static init order dependency.
NameMap<QString, void*>& registryEntries() {
  static NameMap<QString, void*> entries{
      ContainerName{"Composition.RegistryEntries"}};
  return entries;
}
}  // namespace

CompositionRegistry& CompositionRegistry::global() {
  static CompositionRegistry instance;
  return instance;
}

void CompositionRegistry::registerComposition(const QString& name,
                                              void* composition) {
  const QString key = name.trimmed();
  if (key.isEmpty() || composition == nullptr) {
    return;
  }
  registryEntries()[key] = composition;
}

void CompositionRegistry::unregisterComposition(const QString& name,
                                                void* composition) {
  const QString key = name.trimmed();
  if (key.isEmpty() || composition == nullptr) {
    return;
  }
  auto& entries = registryEntries();
  if (!entries.contains(key) || entries[key] != composition) {
    // A stale destructor must not evict a newer composition of the same name.
    return;
  }
  entries.erase(key);
}

void* CompositionRegistry::findComposition(const QString& name) const {
  const QString key = name.trimmed();
  if (key.isEmpty()) {
    return nullptr;
  }
  auto& entries = registryEntries();
  return entries.contains(key) ? entries[key] : nullptr;
}

QStringList CompositionRegistry::registeredNames() const {
  QStringList names;
  names.reserve(static_cast<int>(registryEntries().size()));
  for (const auto& entry : registryEntries()) {
    names.push_back(entry.first);
  }
  return names;
}

void CompositionRegistry::clear() {
  registryEntries().clear();
}

} // namespace ArtifactCore