module;
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
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

    bus.setPublishHook([this](std::type_index type, std::string_view name,
                              std::size_t delivered, std::int64_t durationNs) {
        onPublish(type, name, delivered, durationNs);
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
void EventBusDebugger::clearLog()                                 { std::lock_guard l(mutex_); log_.clear();         }

// ------------------------------------------------------------------ onPublish
void EventBusDebugger::onPublish(std::type_index type, std::string_view name,
                                  std::size_t delivered, std::int64_t durationNs)
{
    if (paused_.load(std::memory_order_relaxed)) return;

    const double nowMs_ = nowMs();
    const std::int64_t durUs = durationNs / 1000;

    std::lock_guard lock(mutex_);
    const double tsMs = nowMs_ - startMs_;
    const double dupWindow = dupWindowMs_;

    auto& state = perEvent_[type];

    // Store event name on first occurrence
    if (state.name.empty() && !name.empty()) {
        state.name = cleanName(name);
    }

    // Duplicate detection: fired within dupWindow of last fire?
    const bool isDup = (state.lastFire >= 0.0) && ((tsMs - state.lastFire) <= dupWindow);
    state.lastFire = tsMs;
    state.totalFires++;

    // 10s rolling window for frequency
    state.recentFires.push_back(nowMs_);
    const double cutoff = nowMs_ - (kFreqWindowSec * 1000.0);
    while (!state.recentFires.empty() && state.recentFires.front() < cutoff) {
        state.recentFires.pop_front();
    }

    // Append to log (ring buffer)
    if (log_.size() >= kMaxLogEntries) {
        log_.erase(log_.begin(), log_.begin() + static_cast<std::ptrdiff_t>(kMaxLogEntries / 4));
    }
    log_.push_back(FireEntry {
        tsMs,
        cleanName(name),
        delivered,
        durUs,
        isDup
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
            if (state.totalFires > 0) {
                auto it = byType.find(t);
                if (it != byType.end()) {
                    it->second.neverFired = false;
                }
                // else: fired but no longer subscribed — include as ghost
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

} // namespace ArtifactCore
