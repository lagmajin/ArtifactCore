module;
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

export module Container.Debug.Registry;

import Container.Debug;

export namespace ArtifactCore {

class ContainerDebugRegistry {
private:
  struct Entry;

public:
  using SnapshotReader = std::function<ContainerDebugSnapshot()>;
  using DebugNoteWriter = std::function<bool(
    std::string, ContainerDebugNoteSeverity, ContainerDebugNoteAuthor)>;

  class Registration {
  public:
    Registration() = default;
    ~Registration() { reset(); }

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    Registration(Registration&& other) noexcept
      : registry_(std::exchange(other.registry_, nullptr))
      , lifetime_(std::move(other.lifetime_))
      , id_(std::move(other.id_))
    {
    }

    Registration& operator=(Registration&& other) noexcept
    {
      if (this == &other) return *this;
      reset();
      registry_ = std::exchange(other.registry_, nullptr);
      lifetime_ = std::move(other.lifetime_);
      id_ = std::move(other.id_);
      return *this;
    }

    void reset() noexcept
    {
      if (registry_ != nullptr && !lifetime_.expired()) registry_->unregisterReader(id_);
      registry_ = nullptr;
      lifetime_.reset();
      id_.clear();
    }

    explicit operator bool() const noexcept
    {
      return registry_ != nullptr && !lifetime_.expired();
    }

  private:
    friend class ContainerDebugRegistry;

    Registration(ContainerDebugRegistry* registry, std::shared_ptr<void> lifetime, std::string id)
      : registry_(registry)
      , lifetime_(std::move(lifetime))
      , id_(std::move(id))
    {
    }

    ContainerDebugRegistry* registry_ = nullptr;
    std::weak_ptr<void> lifetime_;
    std::string id_;
  };

  ContainerDebugRegistry() = default;
  ContainerDebugRegistry(const ContainerDebugRegistry&) = delete;
  ContainerDebugRegistry& operator=(const ContainerDebugRegistry&) = delete;
  ContainerDebugRegistry(ContainerDebugRegistry&&) = delete;
  ContainerDebugRegistry& operator=(ContainerDebugRegistry&&) = delete;

  static ContainerDebugRegistry& instance()
  {
    static ContainerDebugRegistry registry;
    return registry;
  }

  bool registerReader(std::string id, SnapshotReader reader, DebugNoteWriter noteWriter = {})
  {
    if (id.empty() || !reader) return false;
    std::lock_guard lock(mutex_);
    if (containsUnlocked(id)) return false;
    entries_.push_back(std::make_shared<Entry>(
      std::move(id), std::move(reader), std::move(noteWriter)));
    return true;
  }

  Registration registerScopedReader(std::string id, SnapshotReader reader)
  {
    const std::string registrationId = id;
    if (!registerReader(id, std::move(reader))) return {};
    return Registration(this, lifetime_, registrationId);
  }

  Registration registerScoped(std::string id, SnapshotReader reader, DebugNoteWriter noteWriter)
  {
    const std::string registrationId = id;
    if (!registerReader(id, std::move(reader), std::move(noteWriter))) return {};
    return Registration(this, lifetime_, registrationId);
  }

  bool unregisterReader(const std::string& id)
  {
    std::shared_ptr<Entry> entry;
    {
      std::lock_guard lock(mutex_);
      const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&id](const std::shared_ptr<Entry>& candidate) { return candidate->id == id; });
      if (found == entries_.end()) return false;
      entry = *found;
      entries_.erase(found);
    }
    std::unique_lock lock(entry->inspectionMutex);
    entry->retiring = true;
    entry->activeUsesSettled.wait(lock, [&entry]() { return entry->activeUses == 0; });
    return true;
  }

  bool inspect(const std::string& id, ContainerDebugSnapshot& out) const
  {
    std::shared_ptr<Entry> entry;
    {
      std::lock_guard lock(mutex_);
      const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&id](const std::shared_ptr<Entry>& candidate) { return candidate->id == id; });
      if (found == entries_.end()) return false;
      entry = *found;
    }
    {
      std::lock_guard lock(entry->inspectionMutex);
      if (entry->retiring) return false;
      ++entry->activeUses;
    }
    try {
      out = entry->reader();
    } catch (...) {
      finishInspection(entry);
      return false;
    }
    finishInspection(entry);
    return true;
  }

  bool annotate(const std::string& id,
                std::string text,
                ContainerDebugNoteSeverity severity,
                ContainerDebugNoteAuthor author)
  {
    constexpr std::size_t maxNoteBytes = 1024;
    if (text.empty() || text.size() > maxNoteBytes ||
        !isValidContainerDebugNoteSeverity(severity) ||
        !isValidContainerDebugNoteAuthor(author)) {
      return false;
    }
    std::shared_ptr<Entry> entry;
    {
      std::lock_guard lock(mutex_);
      const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&id](const std::shared_ptr<Entry>& candidate) { return candidate->id == id; });
      if (found == entries_.end()) return false;
      entry = *found;
    }
    {
      std::lock_guard lock(entry->inspectionMutex);
      if (entry->retiring || !entry->noteWriter) return false;
      ++entry->activeUses;
    }
    try {
      const bool written = entry->noteWriter(std::move(text), severity, author);
      finishInspection(entry);
      return written;
    } catch (...) {
      finishInspection(entry);
      return false;
    }
  }

  std::vector<std::string> registeredIds() const
  {
    std::lock_guard lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(entries_.size());
    for (const auto& entry : entries_) ids.push_back(entry->id);
    return ids;
  }

  bool contains(const std::string& id) const
  {
    std::lock_guard lock(mutex_);
    return containsUnlocked(id);
  }

private:
  bool containsUnlocked(const std::string& id) const noexcept
  {
    return std::any_of(entries_.begin(), entries_.end(),
      [&id](const std::shared_ptr<Entry>& entry) { return entry->id == id; });
  }

  struct Entry {
    Entry(std::string entryId, SnapshotReader entryReader, DebugNoteWriter entryNoteWriter)
      : id(std::move(entryId))
      , reader(std::move(entryReader))
      , noteWriter(std::move(entryNoteWriter))
    {
    }

    std::string id;
    SnapshotReader reader;
    DebugNoteWriter noteWriter;
    std::mutex inspectionMutex;
    std::condition_variable activeUsesSettled;
    bool retiring = false;
    std::size_t activeUses = 0;
  };

  static void finishInspection(const std::shared_ptr<Entry>& entry) noexcept
  {
    std::lock_guard lock(entry->inspectionMutex);
    --entry->activeUses;
    if (entry->activeUses == 0) entry->activeUsesSettled.notify_all();
  }

  std::vector<std::shared_ptr<Entry>> entries_;
  mutable std::mutex mutex_;
  std::shared_ptr<void> lifetime_ = std::make_shared<int>(0);
};

/**
 * @brief debugSnapshot()を持つ任意のコンテナをregistryへ登録する共通ヘルパー
 *
 * Registrationはcontainerより先に破棄すること。
 */
template <typename Container>
ContainerDebugRegistry::Registration registerContainerDebugSnapshot(
  ContainerDebugRegistry& registry,
  std::string id,
  const Container& container)
{
  return registry.registerScopedReader(
    std::move(id),
    [&container]() { return container.debugSnapshot(); });
}

}
