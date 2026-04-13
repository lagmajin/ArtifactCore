module;
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>


module Event.Bus;

namespace ArtifactCore {

struct EventBus::Impl {
  struct Entry {
    mutable std::mutex mutex;
    std::vector<std::shared_ptr<SubscriberRecord>> subscribers;
  };

  mutable std::mutex registryMutex;
  std::unordered_map<std::type_index, std::shared_ptr<Entry>> registry;

  mutable std::mutex queueMutex;
  std::deque<QueuedEvent> queue;

  std::atomic_size_t nextSubscriberId{1};
  std::atomic<PublishHookFn> publishHook{nullptr};
};

static void pruneInactive(
    std::vector<std::shared_ptr<EventBus::SubscriberRecord>> &subscribers) {
  subscribers.erase(
      std::remove_if(
          subscribers.begin(), subscribers.end(),
          [](const std::shared_ptr<EventBus::SubscriberRecord> &record) {
            return !record || !record->active.load(std::memory_order_acquire);
          }),
      subscribers.end());
}

static void disconnectRecordFromImpl(EventBus::Impl &impl, std::type_index type,
                                     std::size_t id) {
  std::shared_ptr<EventBus::Impl::Entry> entry;
  {
    std::lock_guard<std::mutex> lock(impl.registryMutex);
    auto it = impl.registry.find(type);
    if (it == impl.registry.end()) {
      return;
    }
    entry = it->second;
  }

  if (!entry) {
    return;
  }

  bool entryEmpty = false;
  {
    std::lock_guard<std::mutex> lock(entry->mutex);
    auto &subscribers = entry->subscribers;
    subscribers.erase(
        std::remove_if(
            subscribers.begin(), subscribers.end(),
            [id](const std::shared_ptr<EventBus::SubscriberRecord> &candidate) {
              return !candidate || candidate->id == id ||
                     !candidate->active.load(std::memory_order_acquire);
            }),
        subscribers.end());
    entryEmpty = subscribers.empty();
  }

  if (entryEmpty) {
    std::lock_guard<std::mutex> registryLock(impl.registryMutex);
    auto it = impl.registry.find(type);
    if (it != impl.registry.end() && it->second == entry) {
      impl.registry.erase(it);
    }
  }
}

EventBus::Subscription::Subscription(
    std::weak_ptr<Impl> impl, std::shared_ptr<SubscriberRecord> record) noexcept
    : impl_(std::move(impl)), record_(std::move(record)) {}

EventBus::Subscription::~Subscription() { disconnect(); }

EventBus::Subscription &
EventBus::Subscription::operator=(Subscription &&other) noexcept {
  if (this != &other) {
    disconnect();
    impl_ = std::move(other.impl_);
    record_ = std::move(other.record_);
  }
  return *this;
}

void EventBus::Subscription::disconnect() noexcept {
  auto record = std::move(record_);
  if (!record) {
    impl_.reset();
    return;
  }

  const bool wasActive =
      record->active.exchange(false, std::memory_order_acq_rel);
  if (wasActive) {
    if (auto impl = impl_.lock()) {
      disconnectRecordFromImpl(*impl, record->type, record->id);
    }
  }

  impl_.reset();
}

bool EventBus::Subscription::connected() const noexcept {
  return record_ && record_->active.load(std::memory_order_acquire);
}

EventBus::EventBus() : impl_(std::make_shared<Impl>()) {}

EventBus::Subscription
EventBus::subscribeRaw(std::type_index type,
                       std::function<void(const void *)> callback) {
  auto impl = impl_;
  if (!impl) {
    return {};
  }

  auto record = std::make_shared<SubscriberRecord>();
  record->id = impl->nextSubscriberId.fetch_add(1, std::memory_order_relaxed);
  record->type = type;
  record->callback = std::move(callback);

  std::shared_ptr<Impl::Entry> entry;
  {
    std::lock_guard<std::mutex> lock(impl->registryMutex);
    auto &slot = impl->registry[type];
    if (!slot) {
      slot = std::make_shared<Impl::Entry>();
    }
    entry = slot;
  }

  {
    std::lock_guard<std::mutex> lock(entry->mutex);
    pruneInactive(entry->subscribers);
    entry->subscribers.push_back(record);
  }

  return Subscription{impl, std::move(record)};
}

std::size_t EventBus::publishRaw(std::type_index type,
                                 const void *payload) const {
  auto impl = impl_;
  if (!impl) {
    return 0;
  }

  std::shared_ptr<Impl::Entry> entry;
  {
    std::lock_guard<std::mutex> lock(impl->registryMutex);
    auto it = impl->registry.find(type);
    if (it == impl->registry.end()) {
      return 0;
    }
    entry = it->second;
  }

  std::vector<std::shared_ptr<SubscriberRecord>> snapshot;
  {
    std::lock_guard<std::mutex> lock(entry->mutex);
    snapshot = entry->subscribers;
  }

  std::size_t delivered = 0;
  for (const auto &record : snapshot) {
    if (!record || !record->active.load(std::memory_order_acquire) ||
        !record->callback) {
      continue;
    }
    record->callback(payload);
    ++delivered;
  }

  if (auto hook = impl->publishHook.load(std::memory_order_acquire)) {
    hook(type, delivered);
  }

  return delivered;
}

void EventBus::enqueueRaw(std::type_index type,
                          std::shared_ptr<const void> payload,
                          void (*dispatch)(EventBus &, const void *),
                          EventPriority priority) {
  auto impl = impl_;
  if (!impl) {
    return;
  }

  std::lock_guard<std::mutex> lock(impl->queueMutex);
  // Insert in sorted position (high priority first)
  auto pos = impl->queue.begin();
  while (pos != impl->queue.end() &&
         static_cast<int>(pos->priority) >= static_cast<int>(priority)) {
    ++pos;
  }
  impl->queue.insert(pos,
                     QueuedEvent{type, std::move(payload), dispatch, priority});
}

std::size_t EventBus::subscriberCountRaw(std::type_index type) const noexcept {
  auto impl = impl_;
  if (!impl) {
    return 0;
  }

  std::shared_ptr<Impl::Entry> entry;
  {
    std::lock_guard<std::mutex> lock(impl->registryMutex);
    auto it = impl->registry.find(type);
    if (it == impl->registry.end()) {
      return 0;
    }
    entry = it->second;
  }

  std::lock_guard<std::mutex> lock(entry->mutex);
  std::size_t count = 0;
  for (const auto &record : entry->subscribers) {
    if (record && record->active.load(std::memory_order_acquire)) {
      ++count;
    }
  }
  return count;
}

std::size_t EventBus::drain(std::size_t maxEvents) {
  auto impl = impl_;
  if (!impl || maxEvents == 0) {
    return 0;
  }

  std::size_t processed = 0;
  while (processed < maxEvents) {
    std::deque<QueuedEvent> batch;
    {
      std::lock_guard<std::mutex> lock(impl->queueMutex);
      while (!impl->queue.empty() && processed < maxEvents) {
        batch.push_back(std::move(impl->queue.front()));
        impl->queue.pop_front();
        ++processed;
      }
    }

    if (batch.empty()) {
      break;
    }

    for (const auto &queued : batch) {
      if (!queued.dispatch || !queued.payload) {
        continue;
      }
      queued.dispatch(*this, queued.payload.get());
    }
  }

  return processed;
}

void EventBus::clear() {
  auto impl = impl_;
  if (!impl) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(impl->registryMutex);
    for (auto &[type, entry] : impl->registry) {
      (void)type;
      if (!entry) {
        continue;
      }
      std::lock_guard<std::mutex> entryLock(entry->mutex);
      for (auto &record : entry->subscribers) {
        if (record) {
          record->active.store(false, std::memory_order_release);
        }
      }
      entry->subscribers.clear();
    }
    impl->registry.clear();
  }

  clearQueue();
}

void EventBus::clearQueue() {
  auto impl = impl_;
  if (!impl) {
    return;
  }

  std::lock_guard<std::mutex> lock(impl->queueMutex);
  impl->queue.clear();
}

std::size_t EventBus::pendingCount() const noexcept {
  auto impl = impl_;
  if (!impl) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(impl->queueMutex);
  return impl->queue.size();
}

EventBus &globalEventBus() {
  static EventBus instance;
  return instance;
}

void EventBus::setPublishHook(PublishHookFn hook) noexcept {
  if (impl_) {
    impl_->publishHook.store(hook, std::memory_order_release);
  }
}

} // namespace ArtifactCore
