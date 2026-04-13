module;
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <typeindex>
#include <type_traits>

export module Diagnostics.Profiler;

import std;

export namespace ArtifactCore {

enum class ProfileCategory : std::uint8_t {
    Render = 0,
    Composite,
    UI,
    EventBus,
    IO,
    Animation,
    Other
};

struct ScopeRecord {
    std::string      name;
    ProfileCategory  category      = ProfileCategory::Other;
    std::int64_t     startOffsetNs = 0;
    std::int64_t     durationNs    = 0;
    int              depth         = 0;
};

struct FrameRecord {
    std::int64_t  frameIndex      = -1;
    std::int64_t  frameDurationNs = 0;
    int           canvasWidth     = 0;
    int           canvasHeight    = 0;
    bool          isPlayback      = false;

    std::vector<ScopeRecord>              scopes;
    std::unordered_map<std::string, int>  eventCounts;
    std::unordered_map<std::string, int>  eventSubscriberPeak;
};

// Aggregated stats for a single scope over multiple frames.
struct ScopeStats {
    std::string  name;
    double       avgMs   = 0.0;
    double       p95Ms   = 0.0;
    double       maxMs   = 0.0;
    int          samples = 0;
};

// Aggregated frame-time stats over multiple frames.
struct FrameStats {
    double  avgMs   = 0.0;
    double  p95Ms   = 0.0;
    double  maxMs   = 0.0;
    int     samples = 0;
};

class Profiler {
public:
    static Profiler& instance() noexcept;

    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    void beginFrame(std::int64_t frameIndex,
                    int canvasW = 0, int canvasH = 0,
                    bool isPlayback = false) noexcept;
    void endFrame() noexcept;

    void pushScope(const char* name,
                   ProfileCategory cat = ProfileCategory::Other) noexcept;
    void popScope(const char* name) noexcept;

    void recordEventDispatch(std::type_index type,
                             std::size_t subscriberCount) noexcept;

    [[nodiscard]] std::vector<FrameRecord> frameHistory(int maxFrames = 60) const;
    [[nodiscard]] FrameRecord              lastFrameSnapshot() const;

    // --- Aggregated analytics ---

    // Per-scope statistics over last N frames.
    [[nodiscard]] ScopeStats computeScopeStats(std::string_view name,
                                               int historyFrames = 60) const;

    // Frame-level statistics over last N frames.
    [[nodiscard]] FrameStats computeFrameStats(int historyFrames = 60) const;

    // Collect all unique scope names seen in the last N frames.
    [[nodiscard]] std::vector<std::string> knownScopeNames(int historyFrames = 60) const;

    // Structured diagnostic report — human and AI readable.
    // historyFrames: how many past frames to analyze for averages/trends.
    [[nodiscard]] std::string generateDiagnosticReport(int historyFrames = 60) const;

    // --- UI / ambient timers (work without beginFrame/endFrame) ---

    // Record a single elapsed-time sample for a named timer source.
    // Called by ProfileTimer's destructor; safe to call on any thread.
    void recordTimerSample(const char* name,
                           ProfileCategory cat,
                           double elapsedMs) noexcept;

    // Statistics for a named timer source over recent samples (up to maxSamples).
    [[nodiscard]] ScopeStats timerStats(std::string_view name,
                                        int maxSamples = 60) const;

    // All known timer source names.
    [[nodiscard]] std::vector<std::string> knownTimerNames() const;

    void setEventSpamThreshold(int dispatchesPerFrame) noexcept;
    [[nodiscard]] int eventSpamThreshold() const noexcept;

    void setScopeWarningThresholdMs(double ms) noexcept;
    [[nodiscard]] double scopeWarningThresholdMs() const noexcept;

    struct Impl;

private:
    Profiler();
    ~Profiler();

    std::unique_ptr<Impl> impl_;
};

// RAII scope guard — push on construct, pop on destruct.
// Requires beginFrame/endFrame to be active on the same thread.
class ProfileScope {
public:
    explicit ProfileScope(const char* name,
                          ProfileCategory cat = ProfileCategory::Other) noexcept
        : name_(name)
    {
        Profiler::instance().pushScope(name, cat);
    }

    ~ProfileScope() noexcept {
        Profiler::instance().popScope(name_);
    }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    const char* name_;
};

// Ambient timer — measures elapsed time and records a sample without needing
// an active render frame.  Suitable for Qt paint events, model updates, etc.
class ProfileTimer {
public:
    explicit ProfileTimer(const char* name,
                          ProfileCategory cat = ProfileCategory::UI) noexcept
        : name_(name), cat_(cat), startNs_(nowNs())
    {}

    ~ProfileTimer() noexcept {
        if (!Profiler::instance().isEnabled()) return;
        const double ms = static_cast<double>(nowNs() - startNs_) / 1e6;
        Profiler::instance().recordTimerSample(name_, cat_, ms);
    }

    ProfileTimer(const ProfileTimer&) = delete;
    ProfileTimer& operator=(const ProfileTimer&) = delete;

private:
    static std::int64_t nowNs() noexcept {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(
                   high_resolution_clock::now().time_since_epoch())
            .count();
    }

    const char*      name_;
    ProfileCategory  cat_;
    std::int64_t     startNs_;
};

} // namespace ArtifactCore
