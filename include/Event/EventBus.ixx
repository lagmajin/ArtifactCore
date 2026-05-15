module;
#include <concepts>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <source_location>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <typeinfo>
#include <utility>

export module Event.Bus;

export namespace ArtifactCore {

class EventBus {
public:
    struct Impl;
    struct SubscriberRecord {
        std::size_t id = 0;
        std::type_index type = std::type_index(typeid(void));
        std::atomic_bool active { true };
        std::function<void(const void*)> callback;
    };

    enum class EventPriority : int {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    struct QueuedEvent {
        std::type_index type = std::type_index(typeid(void));
        std::shared_ptr<const void> payload;
        void (*dispatch)(EventBus&, const void*, std::source_location) = nullptr;
        EventPriority priority = EventPriority::Normal;
        std::source_location origin = std::source_location::current();

        bool operator<(const QueuedEvent& other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };

    class Subscription {
    public:
        Subscription() noexcept = default;
        ~Subscription();

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept = default;
        Subscription& operator=(Subscription&& other) noexcept;

        void disconnect() noexcept;
        [[nodiscard]] bool connected() const noexcept;
        explicit operator bool() const noexcept { return connected(); }

    private:
        friend class EventBus;

        Subscription(std::weak_ptr<Impl> impl, std::shared_ptr<SubscriberRecord> record) noexcept;

        std::weak_ptr<Impl> impl_;
        std::shared_ptr<SubscriberRecord> record_;
    };

    EventBus();
    ~EventBus() = default;

    EventBus(const EventBus&) = default;
    EventBus& operator=(const EventBus&) = default;
    EventBus(EventBus&&) noexcept = default;
    EventBus& operator=(EventBus&&) noexcept = default;

    template<typename Event, typename Callable>
        requires std::invocable<Callable&, const Event&>
    Subscription subscribe(Callable&& callback) {
        registerTypeNameRaw(std::type_index(typeid(Event)), typeid(Event).name());
        auto wrapped = [fn = std::decay_t<Callable>(std::forward<Callable>(callback))](const void* payload) mutable {
            std::invoke(fn, *static_cast<const Event*>(payload));
        };
        return subscribeRaw(std::type_index(typeid(Event)), std::move(wrapped));
    }

    template<typename Event>
    std::size_t publish(Event&& event,
                        std::source_location origin = std::source_location::current()) {
        using EventType = std::remove_cvref_t<Event>;
        registerTypeNameRaw(std::type_index(typeid(EventType)), typeid(EventType).name());
        return publishRaw(std::type_index(typeid(EventType)), std::addressof(event), origin);
    }

    template<typename Event, typename... Args>
        requires std::constructible_from<std::remove_cvref_t<Event>, Args...>
    void post(EventPriority priority, Args&&... args,
              std::source_location origin = std::source_location::current()) {
        using EventType = std::remove_cvref_t<Event>;
        auto payload = std::make_shared<EventType>(std::forward<Args>(args)...);
        auto rawPayload = std::shared_ptr<const void>(payload, static_cast<const void*>(payload.get()));
        enqueueRaw(std::type_index(typeid(EventType)), std::move(rawPayload), &EventBus::dispatchQueued<EventType>, priority, origin);
    }

    template<typename Event, typename... Args>
        requires std::constructible_from<std::remove_cvref_t<Event>, Args...>
    void post(Args&&... args,
              std::source_location origin = std::source_location::current()) {
        post<Event>(EventPriority::Normal, std::forward<Args>(args)..., origin);
    }

    template<typename Event>
    void post(EventPriority priority, Event&& event,
              std::source_location origin = std::source_location::current()) {
        using EventType = std::remove_cvref_t<Event>;
        auto payload = std::make_shared<EventType>(std::forward<Event>(event));
        auto rawPayload = std::shared_ptr<const void>(payload, static_cast<const void*>(payload.get()));
        enqueueRaw(std::type_index(typeid(EventType)), std::move(rawPayload), &EventBus::dispatchQueued<EventType>, priority, origin);
    }

    template<typename Event>
    void post(Event&& event,
              std::source_location origin = std::source_location::current()) {
        post<Event>(EventPriority::Normal, std::forward<Event>(event), origin);
    }

    [[nodiscard]] std::size_t drain(std::size_t maxEvents = std::numeric_limits<std::size_t>::max());
    void clear();
    void clearQueue();

    [[nodiscard]] std::size_t pendingCount() const noexcept;

    // Debug observability
    using PublishHook = std::function<void(std::type_index, std::string_view /*name*/, std::size_t /*delivered*/, std::int64_t /*durationNs*/, std::source_location /*origin*/)>;
    void setPublishHook(PublishHook hook);
    void clearPublishHook();
    void forEachRegisteredType(const std::function<void(std::type_index, std::string_view, std::size_t)>& fn) const;

    template<typename Event>
    [[nodiscard]] std::size_t subscriberCount() const {
        return subscriberCountRaw(std::type_index(typeid(Event)));
    }

private:
    std::shared_ptr<Impl> impl_;

    void registerTypeNameRaw(std::type_index type, const char* name);
    Subscription subscribeRaw(std::type_index type, std::function<void(const void*)> callback);
    std::size_t publishRaw(std::type_index type, const void* payload,
                           std::source_location origin = std::source_location::current()) const;
    void enqueueRaw(std::type_index type, std::shared_ptr<const void> payload,
                    void (*dispatch)(EventBus&, const void*, std::source_location),
                    EventPriority priority = EventPriority::Normal,
                    std::source_location origin = std::source_location::current());
    std::size_t subscriberCountRaw(std::type_index type) const noexcept;

    template<typename EventType>
    static void dispatchQueued(EventBus& bus, const void* payload, std::source_location origin) {
        bus.registerTypeNameRaw(std::type_index(typeid(EventType)), typeid(EventType).name());
        bus.publishRaw(std::type_index(typeid(EventType)), payload, origin);
    }
};

EventBus& globalEventBus();

} // namespace ArtifactCore
