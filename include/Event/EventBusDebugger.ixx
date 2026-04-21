module;
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>
export module ArtifactCore.Event.EventBusDebugger;

export import Event.Bus;

export namespace ArtifactCore {

struct FireEntry {
    double       timestampMs    = 0.0;   // wall clock ms since debugger attach
    std::string  eventName;
    std::size_t  subscriberCount = 0;
    std::int64_t durationUs     = 0;
    std::source_location origin = std::source_location::current();
    bool         isDuplicate    = false;
    bool         isSlow         = false; // dispatch time >= slowThreshUs
    bool         isBurst        = false; // N fires within burstWindowMs
};

struct SubscriberInfo {
    std::string     eventName;
    std::type_index typeIdx       = std::type_index(typeid(void));
    std::size_t     activeCount   = 0;
    bool            neverFired    = true;
    std::size_t     totalFires    = 0;
    std::int64_t    avgDurationUs = 0;
};

struct FrequencyEntry {
    std::string  eventName;
    double       firesPerSec = 0.0;
    std::size_t  totalFires  = 0;
    bool         isHighFreq  = false;
};

// Per-event aggregate statistics (returned by perEventStats()).
struct PerEventStats {
    std::string   eventName;
    std::size_t   totalFires    = 0;
    double        firesPerSec   = 0.0;
    std::int64_t  avgDurationUs = 0;
    std::int64_t  maxDurationUs = 0;
    std::int64_t  minDurationUs = 0;
    double        lastFireMs    = -1.0;
    bool          isSlowAvg     = false; // average duration >= slowThreshUs
};

// Global aggregate statistics (returned by globalStats()).
struct GlobalStats {
    std::size_t  totalEventsFired    = 0;
    double       uptimeSec           = 0.0;
    double       overallEventsPerSec = 0.0;
    std::string  slowestEventName;
    std::int64_t slowestMaxUs        = 0;
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
    void setSlowThresholdUs(std::int64_t us) noexcept;
    void setBurstWindow(double windowMs, std::size_t minCount) noexcept;

    void clearLog();

    [[nodiscard]] std::vector<FireEntry>       fireLog(bool dupesOnly = false) const;
    [[nodiscard]] std::vector<SubscriberInfo>  subscriberSnapshot() const;
    [[nodiscard]] std::vector<FrequencyEntry>  frequencySnapshot() const;
    [[nodiscard]] std::vector<PerEventStats>   perEventStats() const;
    [[nodiscard]] GlobalStats                  globalStats() const;

private:
    EventBusDebugger() = default;

    void onPublish(std::type_index type, std::string_view name,
                   std::size_t delivered, std::int64_t durationNs,
                   std::source_location origin);

    static std::string cleanName(std::string_view raw);

    struct PerEventState {
        std::string        name;
        std::deque<double> recentFires;   // absolute wall-clock ms timestamps
        double             lastFire       = -1e9;
        std::size_t        totalFires     = 0;
        std::int64_t       durationUsTotal = 0;
        std::int64_t       durationUsMin   = 0;
        std::int64_t       durationUsMax   = 0;
        std::size_t        durationCount   = 0;
    };

    mutable std::mutex   mutex_;
    std::vector<FireEntry>                               log_;
    mutable std::unordered_map<std::type_index, PerEventState, std::hash<std::type_index>> perEvent_;

    EventBus*    attachedBus_   = nullptr;
    double       startMs_       = 0.0;
    std::size_t  totalFired_    = 0;

    double       dupWindowMs_   = 150.0;
    double       highFreqThresh_ = 30.0;
    std::int64_t slowThreshUs_  = 1000;   // 1 ms default
    double       burstWindowMs_ = 100.0;  // 100 ms window
    std::size_t  burstMinCount_ = 3;      // 3 fires in window = burst
    std::atomic<bool> paused_ { false };

    static constexpr std::size_t kMaxLogEntries = 2000;
    static constexpr double      kFreqWindowSec = 10.0;
};

} // namespace ArtifactCore
