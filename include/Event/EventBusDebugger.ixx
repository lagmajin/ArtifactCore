module;
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>
export module ArtifactCore.Event.EventBusDebugger;

export import ArtifactCore.Event.Bus;

export namespace ArtifactCore {

struct FireEntry {
    double      timestampMs  = 0.0;   // wall clock ms since debugger attach
    std::string eventName;
    std::size_t subscriberCount = 0;
    std::int64_t durationUs    = 0;
    bool        isDuplicate    = false;
};

struct SubscriberInfo {
    std::string    eventName;
    std::type_index typeIdx;
    std::size_t    activeCount  = 0;
    bool           neverFired   = true;
};

struct FrequencyEntry {
    std::string eventName;
    double      firesPerSec  = 0.0;
    std::size_t totalFires   = 0;
    bool        isHighFreq   = false;
};

class EventBusDebugger {
public:
    static EventBusDebugger& instance();

    void attach(EventBus& bus);
    void detach();
    bool isAttached() const noexcept;

    void setPaused(bool paused) noexcept;
    void setDuplicateWindowMs(double ms) noexcept;
    void setHighFreqThreshold(double firesPerSec) noexcept;

    void clearLog();

    [[nodiscard]] std::vector<FireEntry>       fireLog(bool dupesOnly = false) const;
    [[nodiscard]] std::vector<SubscriberInfo>  subscriberSnapshot() const;
    [[nodiscard]] std::vector<FrequencyEntry>  frequencySnapshot() const;

private:
    EventBusDebugger() = default;

    void onPublish(std::type_index type, std::string_view name,
                   std::size_t delivered, std::int64_t durationNs);

    static std::string cleanName(std::string_view raw);

    struct PerEventState {
        std::string        name;
        std::deque<double> recentFires;   // timestamps (ms) within 10s window
        double             lastFire   = -1e9;
        std::size_t        totalFires = 0;
    };

    mutable std::mutex   mutex_;
    std::vector<FireEntry>                               log_;
    mutable std::unordered_map<std::type_index, PerEventState, std::hash<std::type_index>> perEvent_;

    EventBus*  attachedBus_   = nullptr;
    double     startMs_       = 0.0;

    double     dupWindowMs_   = 150.0;
    double     highFreqThresh_ = 30.0;
    std::atomic<bool> paused_ { false };

    static constexpr std::size_t kMaxLogEntries = 2000;
    static constexpr double      kFreqWindowSec = 10.0;
};

} // namespace ArtifactCore
