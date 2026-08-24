module;
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

module Event.Bus;

import Memory.SharedPtr;
import Container.NamedVector;

namespace ArtifactCore {

struct EventBus::Impl {
    struct Entry {
        mutable std::mutex mutex;
        NamedVector<SharedPtr<SubscriberRecord>> subscribers;
    };

    mutable std::mutex registryMutex;
    std::unordered_map<std::type_index, SharedPtr<Entry>> registry;

    mutable std::mutex queueMutex;
    std::deque<QueuedEvent> queue;

    std::atomic_size_t nextSubscriberId { 1 };

    // Debug hooks (only active when a debugger is attached)
    mutable std::mutex hookMutex;
    EventBus::PublishHook publishHook;
    mutable std::mutex namesMutex;
    std::unordered_map<std::type_index, std::string> typeNames;
};

static void pruneInactive(NamedVector<SharedPtr<EventBus::SubscriberRecord>>& subscribers)
{
    subscribers.removeIf(
        [](const SharedPtr<EventBus::SubscriberRecord>& record) {
            return !record || !record->active.load(std::memory_order_acquire);
        });
}

static void disconnectRecordFromImpl(EventBus::Impl& impl, std::type_index type, std::size_t id)
{
    SharedPtr<EventBus::Impl::Entry> entry;
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
        auto& subscribers = entry->subscribers;
        subscribers.removeIf(
            [id](const SharedPtr<EventBus::SubscriberRecord>& candidate) {
                return !candidate || candidate->id == id || !candidate->active.load(std::memory_order_acquire);
            });
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

EventBus::Subscription::Subscription(WeakPtr<Impl> impl, SharedPtr<SubscriberRecord> record) noexcept
    : impl_(std::move(impl))
    , record_(std::move(record))
{
}

EventBus::Subscription::~Subscription()
{
    disconnect();
}

EventBus::Subscription& EventBus::Subscription::operator=(Subscription&& other) noexcept
{
    if (this != &other) {
        disconnect();
        impl_ = std::move(other.impl_);
        record_ = std::move(other.record_);
    }
    return *this;
}

void EventBus::Subscription::disconnect() noexcept
{
    auto record = std::move(record_);
    if (!record) {
        impl_.reset();
        return;
    }

    const bool wasActive = record->active.exchange(false, std::memory_order_acq_rel);
    if (wasActive) {
        if (auto impl = impl_.lock()) {
            disconnectRecordFromImpl(*impl, record->type, record->id);
        }
    }

    impl_.reset();
}

bool EventBus::Subscription::connected() const noexcept
{
    return record_ && record_->active.load(std::memory_order_acquire);
}

EventBus::EventBus()
    : impl_(makeShared<Impl>())
{
}

EventBus::Subscription EventBus::subscribeRaw(std::type_index type, std::function<void(const void*)> callback)
{
    auto impl = impl_;
    if (!impl) {
        return {};
    }

    auto record = makeShared<SubscriberRecord>();
    record->id = impl->nextSubscriberId.fetch_add(1, std::memory_order_relaxed);
    record->type = type;
    record->callback = std::move(callback);

    SharedPtr<Impl::Entry> entry;
    {
        std::lock_guard<std::mutex> lock(impl->registryMutex);
        auto& slot = impl->registry[type];
        if (!slot) {
            slot = makeShared<Impl::Entry>();
        }
        entry = slot;
    }

    {
        std::lock_guard<std::mutex> lock(entry->mutex);
        pruneInactive(entry->subscribers);
        entry->subscribers.push_back(record);
    }

    return Subscription { impl, std::move(record) };
}

std::size_t EventBus::publishRaw(std::type_index type, const void* payload,
                                 std::source_location origin) const
{
    auto impl = impl_;
    if (!impl) {
        return 0;
    }

    SharedPtr<Impl::Entry> entry;
    {
        std::lock_guard<std::mutex> lock(impl->registryMutex);
        auto it = impl->registry.find(type);
        if (it == impl->registry.end()) {
            return 0;
        }
        entry = it->second;
    }

    NamedVector<SharedPtr<SubscriberRecord>> snapshot{
        makeNamedVector<SharedPtr<SubscriberRecord>>(ContainerName{"EventBusSubscriberSnapshot"})};
    {
        std::lock_guard<std::mutex> lock(entry->mutex);
        snapshot.reserve(entry->subscribers.size());
        for (const auto& subscriber : entry->subscribers) snapshot.append(subscriber);
    }

    const auto dispatchStart = std::chrono::high_resolution_clock::now();
    std::size_t delivered = 0;
    for (const auto& record : snapshot) {
        if (!record || !record->active.load(std::memory_order_acquire) || !record->callback) {
            continue;
        }
        record->callback(payload);
        ++delivered;
    }
    const std::int64_t dispatchNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - dispatchStart).count();

    // Fire debug hook if attached (copy both to avoid holding locks during callback)
    {
        PublishHook hook_copy;
        {
            std::lock_guard<std::mutex> lock(impl->hookMutex);
            hook_copy = impl->publishHook;
        }
        if (hook_copy) {
            std::string name_copy;
            {
                std::lock_guard<std::mutex> lock(impl->namesMutex);
                auto it = impl->typeNames.find(type);
                if (it != impl->typeNames.end()) name_copy = it->second;
            }
            hook_copy(type, name_copy, delivered, dispatchNs, origin);
        }
    }

    return delivered;
}

void EventBus::enqueueRaw(std::type_index type, SharedPtr<const void> payload,
                          void (*dispatch)(EventBus&, const void*, std::source_location),
                          EventPriority priority,
                          std::source_location origin)
{
    auto impl = impl_;
    if (!impl) {
        return;
    }

    std::lock_guard<std::mutex> lock(impl->queueMutex);
    // Insert in sorted position (high priority first)
    auto pos = impl->queue.begin();
    while (pos != impl->queue.end() && static_cast<int>(pos->priority) >= static_cast<int>(priority)) {
        ++pos;
    }
    impl->queue.insert(pos, QueuedEvent { type, std::move(payload), dispatch, priority, origin });
}

std::size_t EventBus::subscriberCountRaw(std::type_index type) const noexcept
{
    auto impl = impl_;
    if (!impl) {
        return 0;
    }

    SharedPtr<Impl::Entry> entry;
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
    for (const auto& record : entry->subscribers) {
        if (record && record->active.load(std::memory_order_acquire)) {
            ++count;
        }
    }
    return count;
}

void EventBus::registerTypeNameRaw(std::type_index type, const char* name)
{
    auto impl = impl_;
    if (!impl || !name) return;
    std::lock_guard<std::mutex> lock(impl->namesMutex);
    impl->typeNames.emplace(type, name);  // no-op if already present
}

void EventBus::setPublishHook(PublishHook hook)
{
    auto impl = impl_;
    if (!impl) return;
    std::lock_guard<std::mutex> lock(impl->hookMutex);
    impl->publishHook = std::move(hook);
}

void EventBus::clearPublishHook()
{
    auto impl = impl_;
    if (!impl) return;
    std::lock_guard<std::mutex> lock(impl->hookMutex);
    impl->publishHook = nullptr;
}

void EventBus::forEachRegisteredType(
    const std::function<void(std::type_index, std::string_view, std::size_t)>& fn) const
{
    if (!fn) return;
    auto impl = impl_;
    if (!impl) return;

    // Snapshot registry to avoid holding locks while calling fn
    NamedVector<std::pair<std::type_index, SharedPtr<Impl::Entry>>> snapshot{
        makeNamedVector<std::pair<std::type_index, SharedPtr<Impl::Entry>>>(
            ContainerName{"EventBusRegisteredTypeSnapshot"})};
    {
        std::lock_guard<std::mutex> lock(impl->registryMutex);
        snapshot.reserve(impl->registry.size());
        for (const auto& [t, entry] : impl->registry) {
            snapshot.make(t, entry);
        }
    }

    for (const auto& [type, entry] : snapshot) {
        if (!entry) continue;
        std::size_t count = 0;
        {
            std::lock_guard<std::mutex> lock(entry->mutex);
            for (const auto& rec : entry->subscribers) {
                if (rec && rec->active.load(std::memory_order_acquire)) ++count;
            }
        }
        std::string name_str;
        {
            std::lock_guard<std::mutex> lock(impl->namesMutex);
            auto it = impl->typeNames.find(type);
            if (it != impl->typeNames.end()) name_str = it->second;
        }
        fn(type, name_str, count);
    }
}

std::size_t EventBus::drain(std::size_t maxEvents)
{
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

        for (const auto& queued : batch) {
            if (!queued.dispatch || !queued.payload) {
                continue;
            }
            queued.dispatch(*this, queued.payload.get(), queued.origin);
        }
    }

    return processed;
}

void EventBus::clear()
{
    auto impl = impl_;
    if (!impl) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(impl->registryMutex);
        for (auto& [type, entry] : impl->registry) {
            (void)type;
            if (!entry) {
                continue;
            }
            std::lock_guard<std::mutex> entryLock(entry->mutex);
            for (auto& record : entry->subscribers) {
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

void EventBus::clearQueue()
{
    auto impl = impl_;
    if (!impl) {
        return;
    }

    std::lock_guard<std::mutex> lock(impl->queueMutex);
    impl->queue.clear();
}

std::size_t EventBus::pendingCount() const noexcept
{
    auto impl = impl_;
    if (!impl) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(impl->queueMutex);
    return impl->queue.size();
}

EventBus& globalEventBus()
{
    static EventBus instance;
    return instance;
}

} // namespace ArtifactCore
