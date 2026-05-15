module;
#include <utility>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <cmath>
#include <numeric>
#include <string>
#include <string_view>
#include <chrono>
#include <mutex>
#include <map>
#include <vector>
#include <sstream>
export module ArtifactCore.Utils.PerformanceProfiler;

namespace ArtifactCore {

/**
 * @brief Simple performance sample data
 */
export struct PerformanceSample {
    std::string name;
    double durationMs;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Registry to collect and provide performance metrics
 */
export class PerformanceRegistry {
public:
    static PerformanceRegistry& instance() {
        static PerformanceRegistry inst;
        return inst;
    }

    void recordSample(const std::string& name, double durationMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_[name] = { name, durationMs, std::chrono::system_clock::now() };
        
        // Keep a short history for trend analysis if needed
        history_[name].push_back(durationMs);
        if (history_[name].size() > 100) {
            history_[name].erase(history_[name].begin());
        }
    }

    std::map<std::string, PerformanceSample> getLatestSamples() {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_;
    }

    std::vector<double> getHistory(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_[name];
    }

private:
    std::mutex mutex_;
    std::map<std::string, PerformanceSample> samples_;
    std::map<std::string, std::vector<double>> history_;
};

/**
 * @brief RAII timer to measure scope execution time
 */
export class ScopedPerformanceTimer {
public:
    ScopedPerformanceTimer(const std::string& name) 
        : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedPerformanceTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start_;
        PerformanceRegistry::instance().recordSample(name_, elapsed.count());
    }

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

export enum class ProfileCategory : std::uint8_t {
    Render = 0,
    Composite = 1,
    UI = 2,
    EventBus = 3,
    IO = 4,
    Animation = 5,
    Other = 6,
    Audio = 7
};

export class ProfileTimer {
public:
    ProfileTimer(const std::string& name, ProfileCategory category)
        : category_(category), timer_(name) {}

private:
    ProfileCategory category_ = ProfileCategory::Other;
    ScopedPerformanceTimer timer_;
};

export struct ScopeRecord {
    std::string name;
    ProfileCategory category = ProfileCategory::Other;
    int depth = 0;
    std::int64_t durationNs = 0;
};

export struct FrameRecord {
    std::int64_t frameIndex = 0;
    std::int64_t frameDurationNs = 0;
    bool isPlayback = false;
    int canvasWidth = 0;
    int canvasHeight = 0;
    std::vector<ScopeRecord> scopes;
    std::map<std::string, int> eventCounts;
    std::map<std::string, int> eventSubscriberPeak;
};

export struct FrameStats {
    double avgMs = 0.0;
    double p95Ms = 0.0;
    double maxMs = 0.0;
    int samples = 0;
};

export struct ScopeStats {
    double avgMs = 0.0;
    double p95Ms = 0.0;
    double maxMs = 0.0;
    int samples = 0;
};

export class Profiler {
public:
    static Profiler& instance() {
        static Profiler inst;
        return inst;
    }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    void beginFrame(std::int64_t frameIndex, int canvasWidth, int canvasHeight, bool isPlayback) {
        auto& ts = threadState();
        ts.frame = {};
        ts.frame.frameIndex = frameIndex;
        ts.frame.canvasWidth = canvasWidth;
        ts.frame.canvasHeight = canvasHeight;
        ts.frame.isPlayback = isPlayback;
        ts.frameStart = std::chrono::high_resolution_clock::now();
        ts.scopeStack.clear();
        ts.inFrame = enabled_;
        if (!enabled_) {
            ts.frame = {};
        }
    }

    void endFrame() {
        auto& ts = threadState();
        if (!enabled_ || !ts.inFrame) {
            ts.scopeStack.clear();
            ts.inFrame = false;
            return;
        }

        const auto now = std::chrono::high_resolution_clock::now();
        ts.frame.frameDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now - ts.frameStart).count();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            frames_.push_back(ts.frame);
            if (frames_.size() > kMaxFrames) {
                frames_.pop_front();
            }
        }

        ts.scopeStack.clear();
        ts.inFrame = false;
    }

    bool beginScope(std::string_view name, ProfileCategory category) {
        auto& ts = threadState();
        if (!enabled_ || !ts.inFrame) {
            return false;
        }

        ts.scopeStack.push_back(ActiveScope{
            std::string(name),
            category,
            std::chrono::high_resolution_clock::now(),
            static_cast<int>(ts.scopeStack.size())
        });
        return true;
    }

    void endScope() {
        auto& ts = threadState();
        if (!enabled_ || !ts.inFrame || ts.scopeStack.empty()) {
            return;
        }

        const auto now = std::chrono::high_resolution_clock::now();
        auto active = std::move(ts.scopeStack.back());
        ts.scopeStack.pop_back();

        ScopeRecord rec;
        rec.name = std::move(active.name);
        rec.category = active.category;
        rec.depth = active.depth;
        rec.durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now - active.start).count();
        ts.frame.scopes.push_back(std::move(rec));
    }

    void recordEvent(std::string_view name, int subscriberCount = 0) {
        auto& ts = threadState();
        if (!enabled_ || !ts.inFrame) {
            return;
        }

        const std::string key(name);
        ++ts.frame.eventCounts[key];
        auto& peak = ts.frame.eventSubscriberPeak[key];
        if (subscriberCount > peak) {
            peak = subscriberCount;
        }
    }

    std::vector<FrameRecord> frameHistory(int n) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (n <= 0 || frames_.empty()) {
            return {};
        }

        const std::size_t count = std::min<std::size_t>(static_cast<std::size_t>(n), frames_.size());
        return std::vector<FrameRecord>(frames_.end() - static_cast<std::ptrdiff_t>(count), frames_.end());
    }

    FrameRecord lastFrameSnapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.empty() ? FrameRecord{} : frames_.back();
    }

    FrameStats computeFrameStats(int histN) const {
        const auto frames = frameHistory(histN);
        std::vector<double> values;
        values.reserve(frames.size());
        for (const auto& fr : frames) {
            values.push_back(static_cast<double>(fr.frameDurationNs) / 1e6);
        }
        return summarize<FrameStats>(values);
    }

    ScopeStats computeScopeStats(const std::string& name, int histN) const {
        const auto frames = frameHistory(histN);
        std::vector<double> values;
        for (const auto& fr : frames) {
            for (const auto& scope : fr.scopes) {
                if (scope.name == name) {
                    values.push_back(static_cast<double>(scope.durationNs) / 1e6);
                }
            }
        }
        return summarize<ScopeStats>(values);
    }

    std::vector<std::string> knownTimerNames() const {
        const auto latest = PerformanceRegistry::instance().getLatestSamples();
        std::vector<std::string> names;
        names.reserve(latest.size());
        for (const auto& [name, sample] : latest) {
            (void)sample;
            names.push_back(name);
        }
        return names;
    }

    ScopeStats timerStats(const std::string& name, int /*histN*/) const {
        const auto history = PerformanceRegistry::instance().getHistory(name);
        std::vector<double> values(history.begin(), history.end());
        return summarize<ScopeStats>(values);
    }

    double scopeWarningThresholdMs() const { return scopeWarningThresholdMs_; }
    int eventSpamThreshold() const { return eventSpamThreshold_; }

    std::string generateDiagnosticReport(int histN) const {
        std::ostringstream oss;
        const auto last = lastFrameSnapshot();
        const auto fstats = computeFrameStats(histN);
        oss << "Profiler report\n";
        oss << "  enabled=" << (enabled_ ? "true" : "false") << '\n';
        oss << "  lastFrameMs=" << (static_cast<double>(last.frameDurationNs) / 1e6) << '\n';
        oss << "  avgMs=" << fstats.avgMs << " p95Ms=" << fstats.p95Ms << " maxMs=" << fstats.maxMs << '\n';
        oss << "  scopes=" << last.scopes.size() << " events=" << last.eventCounts.size() << '\n';
        for (const auto& name : knownTimerNames()) {
            const auto st = timerStats(name, histN);
            oss << "  timer " << name << " avgMs=" << st.avgMs
                << " p95Ms=" << st.p95Ms << " samples=" << st.samples << '\n';
        }
        return oss.str();
    }

private:
    struct ActiveScope {
        std::string name;
        ProfileCategory category = ProfileCategory::Other;
        std::chrono::high_resolution_clock::time_point start;
        int depth = 0;
    };

    struct ThreadState {
        bool inFrame = false;
        std::int64_t frameIndex = 0;
        std::chrono::high_resolution_clock::time_point frameStart;
        FrameRecord frame;
        std::vector<ActiveScope> scopeStack;
    };

    static ThreadState& threadState() {
        static thread_local ThreadState state;
        return state;
    }

    static constexpr std::size_t kMaxFrames = 300;

    template <typename TStats>
    static TStats summarize(const std::vector<double>& values) {
        TStats stats;
        if (values.empty()) {
            return stats;
        }

        stats.samples = static_cast<int>(values.size());
        const double sum = std::accumulate(values.begin(), values.end(), 0.0);
        stats.avgMs = sum / static_cast<double>(values.size());
        stats.maxMs = *std::max_element(values.begin(), values.end());

        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t idx = static_cast<std::size_t>(
            0.95 * static_cast<double>(sorted.size() - 1));
        stats.p95Ms = sorted[idx];
        return stats;
    }

    mutable std::mutex mutex_;
    std::deque<FrameRecord> frames_;
    bool enabled_ = true;
    double scopeWarningThresholdMs_ = 8.0;
    int eventSpamThreshold_ = 25;
};

// ---------------------------------------------------------------------------
// Startup / one-shot profiler
// ---------------------------------------------------------------------------

export enum class StartupPhase : std::uint8_t {
    DeviceCreation = 0,
    ShaderCompilation,
    PSOCreation,
    PSOCacheLoad,
    PSOCacheSave,
    RayTracingInit,
    PrimitiveRendererInit,
    SwapChainCreation,
    TotalStartup,
    Custom
};

export struct StartupEvent {
    std::string name;
    StartupPhase phase = StartupPhase::Custom;
    double durationMs = 0.0;
    std::chrono::system_clock::time_point timestamp;
    int threadCount = 0;   // number of parallel tasks (0 = sequential)
    int itemCount = 0;     // e.g. shader count, PSO count
};

export class StartupProfiler {
public:
    static StartupProfiler& instance() {
        static StartupProfiler inst;
        return inst;
    }

    void recordEvent(const std::string& name, StartupPhase phase,
                     double durationMs, int threadCount = 0, int itemCount = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(StartupEvent{
            name, phase, durationMs,
            std::chrono::system_clock::now(),
            threadCount, itemCount});
    }

    std::vector<StartupEvent> events() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
    }

    double totalMs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        double total = 0.0;
        for (const auto& e : events_) {
            if (e.phase == StartupPhase::TotalStartup)
                return e.durationMs;
            total += e.durationMs;
        }
        return total;
    }

    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "=== Startup Profile ===\n";
        double sum = 0.0;
        for (const auto& e : events_) {
            oss << "  " << e.name << ": " << e.durationMs << " ms";
            if (e.threadCount > 0)
                oss << " (threads=" << e.threadCount << ")";
            if (e.itemCount > 0)
                oss << " (items=" << e.itemCount << ")";
            oss << "\n";
            if (e.phase != StartupPhase::TotalStartup)
                sum += e.durationMs;
        }
        oss << "  --- sum (excl. total): " << sum << " ms\n";
        return oss.str();
    }

private:
    mutable std::mutex mutex_;
    std::vector<StartupEvent> events_;
};

export class ScopedStartupTimer {
public:
    ScopedStartupTimer(const std::string& name, StartupPhase phase,
                       int threadCount = 0, int itemCount = 0)
        : name_(name), phase_(phase),
          threadCount_(threadCount), itemCount_(itemCount),
          start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedStartupTimer() {
        const auto end = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        StartupProfiler::instance().recordEvent(name_, phase_, ms,
                                                 threadCount_, itemCount_);
    }

    ScopedStartupTimer(const ScopedStartupTimer&) = delete;
    ScopedStartupTimer& operator=(const ScopedStartupTimer&) = delete;

private:
    std::string name_;
    StartupPhase phase_;
    int threadCount_;
    int itemCount_;
    std::chrono::high_resolution_clock::time_point start_;
};

export class ProfileScope {
public:
    ProfileScope(std::string_view name, ProfileCategory category)
        : active_(Profiler::instance().beginScope(name, category)) {}

    ~ProfileScope() {
        if (active_) {
            Profiler::instance().endScope();
        }
    }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
    ProfileScope(ProfileScope&&) = delete;
    ProfileScope& operator=(ProfileScope&&) = delete;

private:
    bool active_ = false;
};

// ---------------------------------------------------------------------------
// Audio Engine Profiler — lock-free, safe from real-time audio callback threads
// ---------------------------------------------------------------------------

export struct AudioCallbackStats {
    double avgCallbackUs  = 0.0;  ///< Average audio callback duration (µs)
    double maxCallbackUs  = 0.0;  ///< Peak audio callback duration (µs)
    double avgFillMs      = 0.0;  ///< Average fill-loop duration per updateAudio call (ms)
    double maxFillMs      = 0.0;  ///< Peak fill-loop duration (ms)
    std::int64_t totalCallbacks  = 0;
    std::int64_t totalUnderflows = 0;
    std::int64_t totalFillLoops  = 0;
    double bufferLevelPct = 0.0;  ///< Ring-buffer fill level relative to target (0-100+)
    int lastCallbackFrames  = 0;
    int lastRequestedFrames = 0;
};

export class AudioEngineProfiler {
public:
    static AudioEngineProfiler& instance() {
        static AudioEngineProfiler inst;
        return inst;
    }

    // Lock-free: safe to call from the real-time audio callback thread.
    void recordCallback(std::int64_t durationNs, int requestedFrames, int providedFrames) {
        callbackSumNs_.fetch_add(durationNs, std::memory_order_relaxed);
        ++callbackCount_;

        std::int64_t prevMax = callbackMaxNs_.load(std::memory_order_relaxed);
        while (durationNs > prevMax) {
            if (callbackMaxNs_.compare_exchange_weak(prevMax, durationNs,
                    std::memory_order_relaxed)) break;
        }

        if (providedFrames < requestedFrames) {
            ++underflowCount_;
        }
        lastCallbackFrames_.store(providedFrames, std::memory_order_relaxed);
        lastRequestedFrames_.store(requestedFrames, std::memory_order_relaxed);
    }

    // Lock-free: safe from any thread.
    void recordFillLoop(std::int64_t durationNs) {
        fillSumNs_.fetch_add(durationNs, std::memory_order_relaxed);
        ++fillCount_;

        std::int64_t prevMax = fillMaxNs_.load(std::memory_order_relaxed);
        while (durationNs > prevMax) {
            if (fillMaxNs_.compare_exchange_weak(prevMax, durationNs,
                    std::memory_order_relaxed)) break;
        }
    }

    // Stores buffer level as pct * 10 integer to avoid fp atomics.
    void setBufferLevel(double pct) {
        const int v = static_cast<int>(std::clamp(pct * 10.0, 0.0, 100000.0));
        bufferLevelPct_.store(v, std::memory_order_relaxed);
    }

    AudioCallbackStats snapshot() const {
        AudioCallbackStats s;
        s.totalCallbacks  = callbackCount_.load(std::memory_order_relaxed);
        s.totalUnderflows = underflowCount_.load(std::memory_order_relaxed);
        s.totalFillLoops  = fillCount_.load(std::memory_order_relaxed);

        const std::int64_t cbSum = callbackSumNs_.load(std::memory_order_relaxed);
        s.avgCallbackUs = (s.totalCallbacks > 0)
            ? static_cast<double>(cbSum) / static_cast<double>(s.totalCallbacks) / 1000.0
            : 0.0;
        s.maxCallbackUs = static_cast<double>(
            callbackMaxNs_.load(std::memory_order_relaxed)) / 1000.0;

        const std::int64_t fillSum = fillSumNs_.load(std::memory_order_relaxed);
        s.avgFillMs = (s.totalFillLoops > 0)
            ? static_cast<double>(fillSum) / static_cast<double>(s.totalFillLoops) / 1e6
            : 0.0;
        s.maxFillMs = static_cast<double>(
            fillMaxNs_.load(std::memory_order_relaxed)) / 1e6;

        s.bufferLevelPct     = static_cast<double>(
            bufferLevelPct_.load(std::memory_order_relaxed)) / 10.0;
        s.lastCallbackFrames  = lastCallbackFrames_.load(std::memory_order_relaxed);
        s.lastRequestedFrames = lastRequestedFrames_.load(std::memory_order_relaxed);
        return s;
    }

    void resetStats() {
        callbackCount_  = 0;
        callbackSumNs_  = 0;
        callbackMaxNs_  = 0;
        fillCount_      = 0;
        fillSumNs_      = 0;
        fillMaxNs_      = 0;
        underflowCount_ = 0;
        bufferLevelPct_ = 0;
    }

private:
    std::atomic<std::int64_t> callbackCount_{0};
    std::atomic<std::int64_t> callbackSumNs_{0};
    std::atomic<std::int64_t> callbackMaxNs_{0};
    std::atomic<std::int64_t> fillCount_{0};
    std::atomic<std::int64_t> fillSumNs_{0};
    std::atomic<std::int64_t> fillMaxNs_{0};
    std::atomic<std::int64_t> underflowCount_{0};
    std::atomic<int>          bufferLevelPct_{0};
    std::atomic<int>          lastCallbackFrames_{0};
    std::atomic<int>          lastRequestedFrames_{0};
};

} // namespace ArtifactCore
