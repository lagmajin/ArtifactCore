module;
#include <concepts>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
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
        void (*dispatch)(EventBus&, const void*) = nullptr;
        EventPriority priority = EventPriority::Normal;

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
        auto wrapped = [fn = std::decay_t<Callable>(std::forward<Callable>(callback))](const void* payload) mutable {
            std::invoke(fn, *static_cast<const Event*>(payload));
        };
        return subscribeRaw(std::type_index(typeid(Event)), std::move(wrapped));
    }

    template<typename Event>
    std::size_t publish(Event&& event) {
        using EventType = std::remove_cvref_t<Event>;
        return publishRaw(std::type_index(typeid(EventType)), std::addressof(event));
    }

    template<typename Event, typename... Args>
        requires std::constructible_from<std::remove_cvref_t<Event>, Args...>
    void post(EventPriority priority, Args&&... args) {
        using EventType = std::remove_cvref_t<Event>;
        auto payload = std::make_shared<EventType>(std::forward<Args>(args)...);
        auto rawPayload = std::shared_ptr<const void>(payload, static_cast<const void*>(payload.get()));
        enqueueRaw(std::type_index(typeid(EventType)), std::move(rawPayload), &EventBus::dispatchQueued<EventType>, priority);
    }

    template<typename Event, typename... Args>
        requires std::constructible_from<std::remove_cvref_t<Event>, Args...>
    void post(Args&&... args) {
        post<Event>(EventPriority::Normal, std::forward<Args>(args)...);
    }

    template<typename Event>
    void post(EventPriority priority, Event&& event) {
        using EventType = std::remove_cvref_t<Event>;
        auto payload = std::make_shared<EventType>(std::forward<Event>(event));
        auto rawPayload = std::shared_ptr<const void>(payload, static_cast<const void*>(payload.get()));
        enqueueRaw(std::type_index(typeid(EventType)), std::move(rawPayload), &EventBus::dispatchQueued<EventType>, priority);
    }

    template<typename Event>
    void post(Event&& event) {
        post<Event>(EventPriority::Normal, std::forward<Event>(event));
    }

    [[nodiscard]] std::size_t drain(std::size_t maxEvents = std::numeric_limits<std::size_t>::max());
    void clear();
    void clearQueue();

    [[nodiscard]] std::size_t pendingCount() const noexcept;

    template<typename Event>
    [[nodiscard]] std::size_t subscriberCount() const {
        return subscriberCountRaw(std::type_index(typeid(Event)));
    }

private:
    std::shared_ptr<Impl> impl_;

    Subscription subscribeRaw(std::type_index type, std::function<void(const void*)> callback);
    std::size_t publishRaw(std::type_index type, const void* payload) const;
    void enqueueRaw(std::type_index type, std::shared_ptr<const void> payload, void (*dispatch)(EventBus&, const void*), EventPriority priority = EventPriority::Normal);
    std::size_t subscriberCountRaw(std::type_index type) const noexcept;

    template<typename EventType>
    static void dispatchQueued(EventBus& bus, const void* payload) {
        bus.publishRaw(std::type_index(typeid(EventType)), payload);
    }
};

EventBus& globalEventBus();

} // namespace ArtifactCore
