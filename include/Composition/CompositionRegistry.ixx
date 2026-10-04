module;
#include <QString>
#include <QStringList>

export module Composition.Registry;

export namespace ArtifactCore {

/// Process-wide name -> composition lookup used by script hosts.
///
/// The registry stores opaque pointers only: it never imports a composition
/// module, so the project layer can register compositions while compositions
/// resolve names without importing the project layer back. Entries must be
/// removed by the owner when the composition dies.
class CompositionRegistry {
public:
  static CompositionRegistry& global();

  /// Registers (or replaces) the entry for a composition name. A later
  /// registration for the same name wins, which matches "most recently opened
  /// composition of that name wins" for scripts.
  void registerComposition(const QString& name, void* composition);

  /// Removes the entry only when it still points at the given composition, so
  /// a stale destructor cannot evict a newer composition with the same name.
  void unregisterComposition(const QString& name, void* composition);

  void* findComposition(const QString& name) const;

  QStringList registeredNames() const;

  void clear();

private:
  CompositionRegistry() = default;
};

} // namespace ArtifactCore