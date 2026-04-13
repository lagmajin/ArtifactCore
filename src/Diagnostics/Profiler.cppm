module;
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

module Diagnostics.Profiler;

import Event.Bus;

namespace ArtifactCore {

namespace {

inline std::int64_t clockNs() noexcept {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
               high_resolution_clock::now().time_since_epoch())
        .count();
}

// Strip MSVC RTTI decoration: "class Foo::BarEvent" → "BarEvent"
std::string cleanTypeName(std::string_view raw) {
    // Remove "class " / "struct " prefix
    for (std::string_view prefix : {"class ", "struct "}) {
        if (raw.substr(0, prefix.size()) == prefix)
            raw.remove_prefix(prefix.size());
    }
    // Take only the last "::" segment
    const auto pos = raw.rfind("::");
    if (pos != std::string_view::npos)
        raw.remove_prefix(pos + 2);
    return std::string(raw);
}

double percentile(std::vector<double>& sorted_vals, double p) {
    if (sorted_vals.empty()) return 0.0;
    const auto idx = static_cast<std::size_t>(p * (sorted_vals.size() - 1));
    return sorted_vals[std::min(idx, sorted_vals.size() - 1)];
}

} // namespace

static constexpr int kHistorySize = 128;

struct PendingScopeEntry {
    const char*      name;
    ProfileCategory  category;
    std::int64_t     startNs;
    int              depth;
    std::size_t      recordIndex;
};

static constexpr int kTimerSamples = 120;

struct TimerEntry {
    ProfileCategory  category = ProfileCategory::UI;
    // Ring buffer of recent samples (ms)
    std::array<double, kTimerSamples> samples{};
    int writeIdx    = 0;
    int validCount  = 0;
};

struct Profiler::Impl {
    std::atomic<bool> enabled{false};

    bool         inFrame      = false;
    std::int64_t frameStartNs = 0;
    FrameRecord  pending;
    std::vector<PendingScopeEntry> scopeStack;

    mutable std::mutex                     historyMutex;
    std::array<FrameRecord, kHistorySize>  history;
    int                                    writeIdx   = 0;
    int                                    validCount = 0;

    // Ambient UI timers (indexed by source name, written from any thread)
    mutable std::mutex                               timerMutex;
    std::unordered_map<std::string, TimerEntry>      timers;

    std::atomic<int>    eventSpamThreshold{10};
    std::atomic<double> scopeWarnMs{5.0};

    Impl() = default;
};

Profiler::Profiler() : impl_(std::make_unique<Impl>()) {}
Profiler::~Profiler() = default;

Profiler& Profiler::instance() noexcept {
    static Profiler inst;
    return inst;
}

void Profiler::setEnabled(bool enabled) noexcept {
    impl_->enabled.store(enabled, std::memory_order_release);
    if (enabled) {
        ArtifactCore::globalEventBus().setPublishHook(
            [](std::type_index type, std::size_t count) {
                Profiler::instance().recordEventDispatch(type, count);
            });
    } else {
        ArtifactCore::globalEventBus().setPublishHook(nullptr);
    }
}

bool Profiler::isEnabled() const noexcept {
    return impl_->enabled.load(std::memory_order_acquire);
}

void Profiler::beginFrame(std::int64_t frameIndex,
                          int canvasW, int canvasH,
                          bool isPlayback) noexcept
{
    if (!impl_->enabled.load(std::memory_order_acquire)) return;
    impl_->inFrame              = true;
    impl_->pending              = {};
    impl_->pending.frameIndex   = frameIndex;
    impl_->pending.canvasWidth  = canvasW;
    impl_->pending.canvasHeight = canvasH;
    impl_->pending.isPlayback   = isPlayback;
    impl_->scopeStack.clear();
    impl_->frameStartNs = clockNs();
}

void Profiler::endFrame() noexcept {
    if (!impl_->enabled.load(std::memory_order_acquire) || !impl_->inFrame)
        return;
    impl_->inFrame = false;
    impl_->pending.frameDurationNs = clockNs() - impl_->frameStartNs;

    std::lock_guard<std::mutex> lk(impl_->historyMutex);
    impl_->history[impl_->writeIdx] = std::move(impl_->pending);
    impl_->writeIdx = (impl_->writeIdx + 1) % kHistorySize;
    if (impl_->validCount < kHistorySize) ++impl_->validCount;
}

void Profiler::pushScope(const char* name, ProfileCategory cat) noexcept {
    if (!impl_->enabled.load(std::memory_order_acquire) || !impl_->inFrame)
        return;
    const int         depth = static_cast<int>(impl_->scopeStack.size());
    const std::size_t idx   = impl_->pending.scopes.size();
    impl_->pending.scopes.push_back(
        {std::string(name), cat, clockNs() - impl_->frameStartNs, 0, depth});
    impl_->scopeStack.push_back({name, cat, clockNs(), depth, idx});
}

void Profiler::popScope(const char* name) noexcept {
    if (!impl_->enabled.load(std::memory_order_acquire) || !impl_->inFrame)
        return;
    for (int i = static_cast<int>(impl_->scopeStack.size()) - 1; i >= 0; --i) {
        if (impl_->scopeStack[i].name == name) {
            const auto& entry      = impl_->scopeStack[i];
            const auto  durationNs = clockNs() - entry.startNs;
            if (entry.recordIndex < impl_->pending.scopes.size())
                impl_->pending.scopes[entry.recordIndex].durationNs = durationNs;
            impl_->scopeStack.erase(impl_->scopeStack.begin() + i);
            return;
        }
    }
}

void Profiler::recordEventDispatch(std::type_index type,
                                   std::size_t subscriberCount) noexcept
{
    if (!impl_->enabled.load(std::memory_order_acquire) || !impl_->inFrame)
        return;
    const std::string name = type.name();
    impl_->pending.eventCounts[name]++;
    auto& peak = impl_->pending.eventSubscriberPeak[name];
    if (static_cast<int>(subscriberCount) > peak)
        peak = static_cast<int>(subscriberCount);
}

std::vector<FrameRecord> Profiler::frameHistory(int maxFrames) const {
    std::lock_guard<std::mutex> lk(impl_->historyMutex);
    const int count = std::min(maxFrames, impl_->validCount);
    if (count == 0) return {};
    std::vector<FrameRecord> result;
    result.reserve(count);
    const int start = (impl_->writeIdx - count + kHistorySize) % kHistorySize;
    for (int i = 0; i < count; ++i)
        result.push_back(impl_->history[(start + i) % kHistorySize]);
    return result;
}

FrameRecord Profiler::lastFrameSnapshot() const {
    std::lock_guard<std::mutex> lk(impl_->historyMutex);
    if (impl_->validCount == 0) return {};
    const int idx = (impl_->writeIdx - 1 + kHistorySize) % kHistorySize;
    return impl_->history[idx];
}

// ---------------------------------------------------------------------------
// Analytics
// ---------------------------------------------------------------------------

ScopeStats Profiler::computeScopeStats(std::string_view name,
                                       int historyFrames) const
{
    const auto frames = frameHistory(historyFrames);
    std::vector<double> durations;
    for (const auto& fr : frames) {
        for (const auto& s : fr.scopes) {
            if (s.name == name)
                durations.push_back(static_cast<double>(s.durationNs) / 1e6);
        }
    }
    if (durations.empty()) return {std::string(name), 0, 0, 0, 0};

    double sum = 0, maxV = 0;
    for (double d : durations) { sum += d; if (d > maxV) maxV = d; }
    std::sort(durations.begin(), durations.end());

    return {
        std::string(name),
        sum / static_cast<double>(durations.size()),
        percentile(durations, 0.95),
        maxV,
        static_cast<int>(durations.size())
    };
}

FrameStats Profiler::computeFrameStats(int historyFrames) const {
    const auto frames = frameHistory(historyFrames);
    if (frames.empty()) return {};

    std::vector<double> durations;
    durations.reserve(frames.size());
    double maxV = 0;
    for (const auto& fr : frames) {
        const double ms = static_cast<double>(fr.frameDurationNs) / 1e6;
        durations.push_back(ms);
        if (ms > maxV) maxV = ms;
    }
    double sum = 0;
    for (double d : durations) sum += d;
    std::sort(durations.begin(), durations.end());

    return {
        sum / static_cast<double>(durations.size()),
        percentile(durations, 0.95),
        maxV,
        static_cast<int>(durations.size())
    };
}

std::vector<std::string> Profiler::knownScopeNames(int historyFrames) const {
    const auto frames = frameHistory(historyFrames);
    std::set<std::string> names;
    for (const auto& fr : frames)
        for (const auto& s : fr.scopes)
            names.insert(s.name);
    return {names.begin(), names.end()};
}

// ---------------------------------------------------------------------------
// Diagnostic report
// ---------------------------------------------------------------------------

std::string Profiler::generateDiagnosticReport(int historyFrames) const {
    constexpr double k60fps = 16.667;
    constexpr double k30fps = 33.333;

    const FrameRecord last  = lastFrameSnapshot();
    const FrameStats  fstat = computeFrameStats(historyFrames);
    const auto        names = knownScopeNames(historyFrames);
    const int         spamT = eventSpamThreshold();

    const double lastMs  = static_cast<double>(last.frameDurationNs) / 1e6;
    const double budgetX = lastMs / k60fps;

    std::ostringstream o;
    o << std::fixed;

    o << "=== ArtifactStudio Performance Diagnostic ===\n";
    o << "Frame #" << last.frameIndex
      << "  Canvas: " << last.canvasWidth << "x" << last.canvasHeight
      << "  Mode: " << (last.isPlayback ? "PLAYBACK" : "IDLE") << "\n\n";

    // --- Frame time ---
    o << "[FRAME TIME]\n";
    o << std::setprecision(1);
    o << "  Last    : " << lastMs << " ms";
    if (lastMs > k30fps)      o << "  *** OVER 30fps BUDGET x" << std::setprecision(2) << budgetX << " ***";
    else if (lastMs > k60fps) o << "  * over 60fps budget x"   << std::setprecision(2) << budgetX;
    o << "\n";
    o << std::setprecision(1);
    if (fstat.samples > 1) {
        o << "  Avg/" << fstat.samples << "f : " << fstat.avgMs << " ms"
          << "  p95: " << fstat.p95Ms << " ms"
          << "  max: " << fstat.maxMs << " ms\n";
        // Trend: compare first half vs second half
        const auto allFrames = frameHistory(historyFrames);
        if (allFrames.size() >= 10) {
            const int half = static_cast<int>(allFrames.size()) / 2;
            double a1 = 0, a2 = 0;
            for (int i = 0; i < half; ++i)
                a1 += static_cast<double>(allFrames[i].frameDurationNs) / 1e6;
            for (int i = half; i < static_cast<int>(allFrames.size()); ++i)
                a2 += static_cast<double>(allFrames[i].frameDurationNs) / 1e6;
            a1 /= half; a2 /= (allFrames.size() - half);
            const double trendPct = (a2 - a1) / std::max(a1, 0.001) * 100.0;
            o << "  Trend   : ";
            if      (trendPct >  10.0) o << "WORSENING  +" << std::setprecision(0) << trendPct << "%\n";
            else if (trendPct < -10.0) o << "IMPROVING  " << std::setprecision(0) << trendPct << "%\n";
            else                        o << "STABLE\n";
        }
    }

    // --- Scope breakdown ---
    o << "\n[SCOPE BREAKDOWN - last frame + avg/" << historyFrames << "f]\n";
    if (names.empty()) {
        o << "  (no scopes recorded)\n";
    } else {
        // Sort scopes by last-frame duration descending
        auto scopes = last.scopes;
        std::sort(scopes.begin(), scopes.end(),
                  [](const ScopeRecord& a, const ScopeRecord& b) {
                      return a.durationNs > b.durationNs;
                  });

        o << "  " << std::left
          << std::setw(22) << "Name"
          << std::setw(9)  << "last"
          << std::setw(9)  << "avg"
          << std::setw(9)  << "p95"
          << std::setw(7)  << "%frame"
          << "\n";
        o << "  " << std::string(56, '-') << "\n";

        for (const auto& s : scopes) {
            const double sMs   = static_cast<double>(s.durationNs) / 1e6;
            const double pct   = lastMs > 0 ? sMs / lastMs * 100.0 : 0.0;
            const auto   stats = computeScopeStats(s.name, historyFrames);
            const std::string indent(static_cast<std::size_t>(s.depth * 2), ' ');
            o << "  " << std::left
              << std::setw(22) << (indent + s.name).substr(0, 22)
              << std::setprecision(1) << std::setw(9) << sMs
              << std::setprecision(1) << std::setw(9) << stats.avgMs
              << std::setprecision(1) << std::setw(9) << stats.p95Ms
              << std::setprecision(1) << std::setw(7) << pct
              << "\n";
        }
    }

    // --- EventBus ---
    o << "\n[EVENTBUS - last frame (event / dispatches / peak_subscribers)]\n";
    if (last.eventCounts.empty()) {
        o << "  (no events this frame)\n";
    } else {
        // Sort by count desc
        std::vector<std::pair<std::string, int>> evts(
            last.eventCounts.begin(), last.eventCounts.end());
        std::sort(evts.begin(), evts.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        int totalDispatches = 0;
        for (const auto& [k, v] : evts) totalDispatches += v;
        o << "  Total dispatches this frame: " << totalDispatches << "\n";

        for (const auto& [rawName, cnt] : evts) {
            const std::string nice = cleanTypeName(rawName);
            const int subs = [&]() -> int {
                auto it = last.eventSubscriberPeak.find(rawName);
                return it != last.eventSubscriberPeak.end() ? it->second : 0;
            }();
            const bool spam = cnt > spamT;
            o << "  " << (spam ? "⚠ " : "  ")
              << std::left << std::setw(36) << nice.substr(0, 36)
              << std::setw(5) << cnt
              << "  subs=" << subs
              << (spam ? "  HIGH" : "")
              << "\n";
        }
    }

    // --- Analysis & suggestions ---
    o << "\n[ANALYSIS]\n";
    int findingIdx = 1;

    // Frame budget
    if (lastMs > k30fps) {
        o << "  " << findingIdx++ << ". [CRITICAL] Frame time " << std::setprecision(1) << lastMs
          << "ms is over 30fps budget (" << k30fps << "ms).\n"
          << "     The UI and rendering are severely degraded.\n"
          << "     Avg=" << fstat.avgMs << "ms suggests this is persistent, not a spike.\n\n";
    } else if (lastMs > k60fps) {
        o << "  " << findingIdx++ << ". [WARNING] Frame time " << std::setprecision(1) << lastMs
          << "ms is over 60fps budget (" << k60fps << "ms).\n\n";
    }

    // Present scope heuristic
    for (const auto& s : last.scopes) {
        if (s.name == "Present") {
            const double pMs  = static_cast<double>(s.durationNs) / 1e6;
            const auto   pSt  = computeScopeStats("Present", historyFrames);
            if (pMs > 8.0) {
                o << "  " << findingIdx++ << ". [GPU STALL] Present=" << std::setprecision(1) << pMs
                  << "ms (avg " << pSt.avgMs << "ms).\n"
                  << "     Hypothesis: GPU is blocking the CPU waiting for vsync or resource fence.\n"
                  << "     Investigate: renderer_->present() call in\n"
                  << "       Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm\n"
                  << "     Try: disable vsync, or add a frame-pacing strategy.\n\n";
            }
        }
    }

    // Sparse instrumentation
    if (names.size() <= 2) {
        o << "  " << findingIdx++ << ". [INFO] Only " << names.size() << " scope(s) active.\n"
          << "     Add ProfileScope markers to narrow down where time is spent:\n"
          << "     - Base pass:   around line 3317 in ArtifactCompositionRenderController.cppm\n"
          << "     - Layer pass:  around line 3465\n"
          << "     - Overlay:     around line 4067\n"
          << "     - Gizmo draw:  around gizmo_->drawGizmo() call\n\n";
    }

    // EventBus spam
    {
        std::vector<std::pair<std::string, int>> spamEvents;
        for (const auto& [k, v] : last.eventCounts)
            if (v > spamT) spamEvents.push_back({k, v});
        std::sort(spamEvents.begin(), spamEvents.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        for (const auto& [rawName, cnt] : spamEvents) {
            const std::string nice = cleanTypeName(rawName);
            o << "  " << findingIdx++ << ". [EVENT CASCADE] " << nice << " fired " << cnt
              << "x this frame (threshold=" << spamT << ").\n"
              << "     Hypothesis: A property change or invalidation triggers a cascade.\n"
              << "     Search for: publish<" << nice << "> or bus.publish(" << nice << ")\n"
              << "     Check if the event handler itself publishes back to the bus.\n\n";
        }
    }

    if (findingIdx == 1) {
        o << "  No issues detected in last frame. Performance looks healthy.\n";
    }

    // --- AI section ---
    o << "\n[FOR AI ASSISTANT]\n";
    o << "  This report was generated by ArtifactStudio's built-in profiler.\n";
    o << "  Key source files:\n";
    o << "    Render loop:  Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm\n";
    o << "    EventBus:     ArtifactCore/src/Event/EventBus.cppm\n";
    o << "    Profiler:     ArtifactCore/include/Diagnostics/Profiler.ixx\n";
    o << "    Event types:  Artifact/include/Event/  (search for struct *Event)\n";
    o << "  Add instrumentation:\n";
    o << "    In .cppm files: import Diagnostics.Profiler;\n";
    o << "    Usage: ArtifactCore::ProfileScope _p(\"MyScope\", ArtifactCore::ProfileCategory::Render);\n";
    o << "  Toggle overlay: Ctrl+Shift+P in CompositeEditor\n";
    o << "  Copy this report: Ctrl+Shift+C\n";

    return o.str();
}

// ---------------------------------------------------------------------------
// Ambient timer API
// ---------------------------------------------------------------------------

void Profiler::recordTimerSample(const char* name,
                                 ProfileCategory cat,
                                 double elapsedMs) noexcept
{
    if (!impl_->enabled.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lk(impl_->timerMutex);
    auto& entry = impl_->timers[name];
    entry.category = cat;
    entry.samples[entry.writeIdx] = elapsedMs;
    entry.writeIdx = (entry.writeIdx + 1) % kTimerSamples;
    if (entry.validCount < kTimerSamples) ++entry.validCount;
}

ScopeStats Profiler::timerStats(std::string_view name, int maxSamples) const {
    std::lock_guard<std::mutex> lk(impl_->timerMutex);
    auto it = impl_->timers.find(std::string(name));
    if (it == impl_->timers.end()) return {std::string(name), 0, 0, 0, 0};

    const auto& e = it->second;
    const int count = std::min(maxSamples, e.validCount);
    if (count == 0) return {std::string(name), 0, 0, 0, 0};

    std::vector<double> durations;
    durations.reserve(count);
    const int start = (e.writeIdx - count + kTimerSamples) % kTimerSamples;
    for (int i = 0; i < count; ++i)
        durations.push_back(e.samples[(start + i) % kTimerSamples]);

    double sum = 0, maxV = 0;
    for (double d : durations) { sum += d; if (d > maxV) maxV = d; }
    std::sort(durations.begin(), durations.end());

    return {
        std::string(name),
        sum / static_cast<double>(count),
        percentile(durations, 0.95),
        maxV,
        count
    };
}

std::vector<std::string> Profiler::knownTimerNames() const {
    std::lock_guard<std::mutex> lk(impl_->timerMutex);
    std::vector<std::string> names;
    names.reserve(impl_->timers.size());
    for (const auto& [k, _] : impl_->timers)
        names.push_back(k);
    std::sort(names.begin(), names.end());
    return names;
}

void Profiler::setEventSpamThreshold(int n) noexcept {
    impl_->eventSpamThreshold.store(n, std::memory_order_release);
}

int Profiler::eventSpamThreshold() const noexcept {
    return impl_->eventSpamThreshold.load(std::memory_order_acquire);
}

void Profiler::setScopeWarningThresholdMs(double ms) noexcept {
    impl_->scopeWarnMs.store(ms, std::memory_order_release);
}

double Profiler::scopeWarningThresholdMs() const noexcept {
    return impl_->scopeWarnMs.load(std::memory_order_acquire);
}

} // namespace ArtifactCore
