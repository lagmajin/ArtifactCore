module;
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>
module ArtifactCore.Event.EventBusDebugger;

namespace ArtifactCore {

static double nowMs()
{
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()) / 1000.0;
}

// ------------------------------------------------------------------ instance
EventBusDebugger& EventBusDebugger::instance()
{
    static EventBusDebugger s_instance;
    return s_instance;
}

// ------------------------------------------------------------------ attach
void EventBusDebugger::attach(EventBus& bus)
{
    std::lock_guard lock(mutex_);
    if (attachedBus_) {
        attachedBus_->clearPublishHook();
    }
    attachedBus_ = &bus;
    startMs_     = nowMs();
    log_.clear();
    perEvent_.clear();
    totalFired_  = 0;

    bus.setPublishHook([this](std::type_index type, std::string_view name,
                              std::size_t delivered, std::int64_t durationNs,
                              std::source_location origin) {
        onPublish(type, name, delivered, durationNs, origin);
    });
}

void EventBusDebugger::detach()
{
    std::lock_guard lock(mutex_);
    if (attachedBus_) {
        attachedBus_->clearPublishHook();
        attachedBus_ = nullptr;
    }
}

bool EventBusDebugger::isAttached() const noexcept
{
    std::lock_guard lock(mutex_);
    return attachedBus_ != nullptr;
}

// ------------------------------------------------------------------ settings
void EventBusDebugger::setPaused(bool paused) noexcept          { paused_.store(paused, std::memory_order_relaxed); }
void EventBusDebugger::setDuplicateWindowMs(double ms) noexcept  { std::lock_guard l(mutex_); dupWindowMs_    = ms;  }
void EventBusDebugger::setHighFreqThreshold(double fps) noexcept { std::lock_guard l(mutex_); highFreqThresh_ = fps; }
void EventBusDebugger::setSlowThresholdUs(std::int64_t us) noexcept { std::lock_guard l(mutex_); slowThreshUs_  = us; }
void EventBusDebugger::setBurstWindow(double windowMs, std::size_t minCount) noexcept
{
    std::lock_guard l(mutex_);
    burstWindowMs_ = windowMs;
    burstMinCount_ = minCount;
}
void EventBusDebugger::clearLog()                                 { std::lock_guard l(mutex_); log_.clear();         }

// ------------------------------------------------------------------ onPublish
void EventBusDebugger::onPublish(std::type_index type, std::string_view name,
                                  std::size_t delivered, std::int64_t durationNs,
                                  std::source_location origin)
{
    if (paused_.load(std::memory_order_relaxed)) return;

    const double nowMs_ = nowMs();
    const std::int64_t durUs = durationNs / 1000;

    std::lock_guard lock(mutex_);
    const double tsMs = nowMs_ - startMs_;

    auto& state = perEvent_[type];

    if (state.name.empty() && !name.empty()) {
        state.name = cleanName(name);
    }

    // Duplicate detection
    const bool isDup = (state.lastFire >= 0.0) && ((tsMs - state.lastFire) <= dupWindowMs_);
    state.lastFire = tsMs;
    state.totalFires++;
    totalFired_++;

    // Duration tracking
    if (durUs > 0) {
        state.durationUsTotal += durUs;
        state.durationCount++;
        if (state.durationCount == 1) {
            state.durationUsMin = durUs;
            state.durationUsMax = durUs;
        } else {
            if (durUs < state.durationUsMin) state.durationUsMin = durUs;
            if (durUs > state.durationUsMax) state.durationUsMax = durUs;
        }
    }

    // Slow flag
    const bool isSlow = (slowThreshUs_ > 0) && (durUs >= slowThreshUs_);

    // Rolling frequency window (10 s)
    state.recentFires.push_back(nowMs_);
    const double freqCutoff = nowMs_ - (kFreqWindowSec * 1000.0);
    while (!state.recentFires.empty() && state.recentFires.front() < freqCutoff) {
        state.recentFires.pop_front();
    }

    // Burst flag: >= burstMinCount_ fires within burstWindowMs_ (including this one)
    bool isBurst = false;
    if (burstMinCount_ > 1) {
        const double burstCutoff = nowMs_ - burstWindowMs_;
        std::size_t cnt = 0;
        for (auto it = state.recentFires.rbegin(); it != state.recentFires.rend(); ++it) {
            if (*it < burstCutoff) break;
            if (++cnt >= burstMinCount_) { isBurst = true; break; }
        }
    }

    // Append to log (ring buffer – trim oldest quarter when full)
    if (log_.size() >= kMaxLogEntries) {
        log_.erase(log_.begin(), log_.begin() + static_cast<std::ptrdiff_t>(kMaxLogEntries / 4));
    }
    log_.push_back(FireEntry {
        tsMs,
        cleanName(name),
        delivered,
        durUs,
        origin,
        isDup,
        isSlow,
        isBurst
    });
}

// ------------------------------------------------------------------ cleanName
std::string EventBusDebugger::cleanName(std::string_view raw)
{
    std::string s(raw);
    // Strip "struct " / "class " prefix (MSVC typeid format)
    for (const char* prefix : { "struct ", "class " }) {
        if (s.starts_with(prefix)) {
            s.erase(0, std::string_view(prefix).size());
            break;
        }
    }
    // Strip leading namespace up to last "::"
    const auto pos = s.rfind("::");
    if (pos != std::string::npos) {
        s.erase(0, pos + 2);
    }
    return s;
}

// ------------------------------------------------------------------ fireLog
std::vector<FireEntry> EventBusDebugger::fireLog(bool dupesOnly) const
{
    std::lock_guard lock(mutex_);
    if (!dupesOnly) return log_;
    std::vector<FireEntry> out;
    out.reserve(log_.size());
    for (const auto& e : log_) {
        if (e.isDuplicate) out.push_back(e);
    }
    return out;
}

// ------------------------------------------------------------------ subscriberSnapshot
std::vector<SubscriberInfo> EventBusDebugger::subscriberSnapshot() const
{
    std::vector<SubscriberInfo> out;

    EventBus* bus = nullptr;
    {
        std::lock_guard lock(mutex_);
        bus = attachedBus_;
    }

    std::unordered_map<std::type_index, SubscriberInfo, std::hash<std::type_index>> byType;

    if (bus) {
        bus->forEachRegisteredType([&](std::type_index t, std::string_view name, std::size_t count) {
            SubscriberInfo info;
            info.typeIdx     = t;
            info.eventName   = cleanName(name);
            info.activeCount = count;
            info.neverFired  = true;
            byType.emplace(t, info);
        });
    }

    {
        std::lock_guard lock(mutex_);
        for (const auto& [t, state] : perEvent_) {
            auto it = byType.find(t);
            if (it != byType.end()) {
                if (state.totalFires > 0) {
                    it->second.neverFired    = false;
                    it->second.totalFires    = state.totalFires;
                    it->second.avgDurationUs = state.durationCount > 0
                        ? (state.durationUsTotal / static_cast<std::int64_t>(state.durationCount))
                        : 0;
                }
            }
        }
    }

    out.reserve(byType.size());
    for (auto& [t, info] : byType) {
        out.push_back(std::move(info));
    }
    std::sort(out.begin(), out.end(), [](const SubscriberInfo& a, const SubscriberInfo& b) {
        return a.eventName < b.eventName;
    });
    return out;
}

// ------------------------------------------------------------------ frequencySnapshot
std::vector<FrequencyEntry> EventBusDebugger::frequencySnapshot() const
{
    std::lock_guard lock(mutex_);
    const double nowMs_ = nowMs();
    const double cutoff = nowMs_ - (kFreqWindowSec * 1000.0);
    const double thresh = highFreqThresh_;

    std::vector<FrequencyEntry> out;
    out.reserve(perEvent_.size());

    for (const auto& [type, state] : perEvent_) {
        if (state.totalFires == 0) continue;

        // Count fires inside window
        std::size_t windowCount = 0;
        for (const auto& ts : state.recentFires) {
            if (ts >= cutoff) ++windowCount;
        }
        const double fps = static_cast<double>(windowCount) / kFreqWindowSec;

        FrequencyEntry fe;
        fe.eventName   = state.name;
        fe.firesPerSec = fps;
        fe.totalFires  = state.totalFires;
        fe.isHighFreq  = fps >= thresh;
        out.push_back(std::move(fe));
    }
    std::sort(out.begin(), out.end(), [](const FrequencyEntry& a, const FrequencyEntry& b) {
        return a.firesPerSec > b.firesPerSec;
    });
    return out;
}

// ------------------------------------------------------------------ perEventStats
std::vector<PerEventStats> EventBusDebugger::perEventStats() const
{
    std::lock_guard lock(mutex_);
    const double now = nowMs();
    const double cutoff = now - (kFreqWindowSec * 1000.0);

    std::vector<PerEventStats> out;
    out.reserve(perEvent_.size());

    for (const auto& [type, state] : perEvent_) {
        if (state.totalFires == 0) continue;

        std::size_t windowCount = 0;
        for (const auto& ts : state.recentFires) {
            if (ts >= cutoff) ++windowCount;
        }
        const double fps = static_cast<double>(windowCount) / kFreqWindowSec;

        PerEventStats s;
        s.eventName     = state.name;
        s.totalFires    = state.totalFires;
        s.firesPerSec   = fps;
        s.lastFireMs    = state.lastFire;
        s.maxDurationUs = state.durationUsMax;
        s.minDurationUs = state.durationCount > 0 ? state.durationUsMin : 0;
        s.avgDurationUs = state.durationCount > 0
            ? (state.durationUsTotal / static_cast<std::int64_t>(state.durationCount))
            : 0;
        s.isSlowAvg = (slowThreshUs_ > 0) && (s.avgDurationUs >= slowThreshUs_);
        out.push_back(std::move(s));
    }
    std::sort(out.begin(), out.end(), [](const PerEventStats& a, const PerEventStats& b) {
        return a.totalFires > b.totalFires;
    });
    return out;
}

// ------------------------------------------------------------------ globalStats
GlobalStats EventBusDebugger::globalStats() const
{
    std::lock_guard lock(mutex_);
    const double elapsedSec = (nowMs() - startMs_) / 1000.0;

    GlobalStats gs;
    gs.totalEventsFired    = totalFired_;
    gs.uptimeSec           = elapsedSec;
    gs.overallEventsPerSec = (elapsedSec > 0.0)
        ? (static_cast<double>(totalFired_) / elapsedSec)
        : 0.0;

    for (const auto& [type, state] : perEvent_) {
        if (state.durationUsMax > gs.slowestMaxUs) {
            gs.slowestMaxUs       = state.durationUsMax;
            gs.slowestEventName   = state.name;
        }
    }
    return gs;
}

} // namespace ArtifactCore
