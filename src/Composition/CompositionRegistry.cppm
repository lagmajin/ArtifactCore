module;
#include <QString>
#include <QStringList>

module Composition.Registry;

import Container.NameMap;

namespace ArtifactCore {

namespace {
/// Compositions retained by other singletons can unregister during CRT teardown.
/// Keep the store alive until process termination so their destructors never
/// access a map that has already been destroyed. Allocate only on first use.
NameMap<QString, void*>& registryEntries() {
  static auto* const entries = new NameMap<QString, void*>{
      ContainerName{"Composition.RegistryEntries"}};
  return *entries;
}
}  // namespace

CompositionRegistry& CompositionRegistry::global() {
  // Match the store's process lifetime, including late composition destruction.
  static auto* const instance = new CompositionRegistry();
  return *instance;
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
