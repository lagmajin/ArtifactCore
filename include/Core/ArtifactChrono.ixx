module;
#include <cstdint>
#include <cmath>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <ctime>
#endif

export module Core.ArtifactChrono;

export namespace ArtifactCore {

// Nanosecond-resolution signed duration. Replaces the common
// std::chrono::duration usages (construction + conversion + arithmetic).
class Duration {
public:
    constexpr Duration() noexcept = default;

    [[nodiscard]] static constexpr Duration fromNanos(const std::int64_t nanos) noexcept {
        return Duration{nanos};
    }
    [[nodiscard]] static Duration fromMicros(const double micros) noexcept {
        return Duration{static_cast<std::int64_t>(std::llround(micros * 1'000.0))};
    }
    [[nodiscard]] static Duration fromMillis(const double millis) noexcept {
        return Duration{static_cast<std::int64_t>(std::llround(millis * 1'000'000.0))};
    }
    [[nodiscard]] static Duration fromSeconds(const double seconds) noexcept {
        return Duration{static_cast<std::int64_t>(std::llround(seconds * 1e9))};
    }

    [[nodiscard]] constexpr std::int64_t nanos() const noexcept { return nanos_; }
    [[nodiscard]] double toMicros() const noexcept { return static_cast<double>(nanos_) / 1'000.0; }
    [[nodiscard]] double toMillis() const noexcept { return static_cast<double>(nanos_) / 1'000'000.0; }
    [[nodiscard]] double toSeconds() const noexcept { return static_cast<double>(nanos_) / 1e9; }

    [[nodiscard]] constexpr Duration operator+(const Duration& other) const noexcept {
        return Duration{nanos_ + other.nanos_};
    }
    [[nodiscard]] constexpr Duration operator-(const Duration& other) const noexcept {
        return Duration{nanos_ - other.nanos_};
    }
    [[nodiscard]] constexpr Duration operator*(const double scalar) const noexcept {
        return Duration{static_cast<std::int64_t>(static_cast<double>(nanos_) * scalar)};
    }
    [[nodiscard]] constexpr Duration operator/(const std::int64_t divisor) const noexcept {
        return divisor == 0 ? Duration{} : Duration{nanos_ / divisor};
    }
    constexpr Duration& operator+=(const Duration& other) noexcept { nanos_ += other.nanos_; return *this; }
    constexpr Duration& operator-=(const Duration& other) noexcept { nanos_ -= other.nanos_; return *this; }

    [[nodiscard]] friend constexpr bool operator==(const Duration& a, const Duration& b) noexcept { return a.nanos_ == b.nanos_; }
    [[nodiscard]] friend constexpr bool operator!=(const Duration& a, const Duration& b) noexcept { return a.nanos_ != b.nanos_; }
    [[nodiscard]] friend constexpr bool operator<(const Duration& a, const Duration& b) noexcept { return a.nanos_ < b.nanos_; }
    [[nodiscard]] friend constexpr bool operator<=(const Duration& a, const Duration& b) noexcept { return a.nanos_ <= b.nanos_; }
    [[nodiscard]] friend constexpr bool operator>(const Duration& a, const Duration& b) noexcept { return a.nanos_ > b.nanos_; }
    [[nodiscard]] friend constexpr bool operator>=(const Duration& a, const Duration& b) noexcept { return a.nanos_ >= b.nanos_; }

private:
    constexpr explicit Duration(const std::int64_t nanos) noexcept : nanos_(nanos) {}
    std::int64_t nanos_ = 0;
};

// Monotonic clock — never goes backwards, immune to system wall-clock edits.
// Tick values are opaque; only differences through between()/elapsed() are
// meaningful.
class SteadyClock {
public:
    using Tick = std::int64_t;

    [[nodiscard]] static Tick now() noexcept {
#if defined(_WIN32)
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return static_cast<Tick>(counter.QuadPart);
#else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<Tick>(ts.tv_sec) * 1'000'000'000LL +
               static_cast<Tick>(ts.tv_nsec);
#endif
    }

    // Seconds per tick; constant per process after first call.
    [[nodiscard]] static double secondsPerTick() noexcept {
#if defined(_WIN32)
        static const double inverse = [] {
            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);
            return frequency.QuadPart > 0
                       ? 1.0 / static_cast<double>(frequency.QuadPart)
                       : 0.0;
        }();
        return inverse;
#else
        return 1e-9;
#endif
    }

    // to - from, clamped at zero for reversed arguments.
    [[nodiscard]] static Duration between(const Tick from, const Tick to) noexcept {
#if defined(_WIN32)
        const double seconds =
            static_cast<double>(to - from) * secondsPerTick();
        return Duration::fromNanos(
            static_cast<std::int64_t>(seconds >= 0.0 ? seconds * 1e9 + 0.5 : 0.0));
#else
        return Duration::fromNanos(to > from ? to - from : 0);
#endif
    }

    [[nodiscard]] static Duration elapsed(const Tick from) noexcept {
        return between(from, now());
    }
};

// Accumulating stopwatch for profiling-style measurements.
class Stopwatch {
public:
    void start() {
        if (!running_) {
            started_ = SteadyClock::now();
            running_ = true;
        }
    }
    void stop() {
        if (running_) {
            accumulated_ += SteadyClock::elapsed(started_);
            running_ = false;
        }
    }
    void restart() {
        accumulated_ = Duration{};
        started_ = SteadyClock::now();
        running_ = true;
    }
    void reset() {
        accumulated_ = Duration{};
        running_ = false;
    }
    [[nodiscard]] Duration elapsed() const noexcept {
        return running_ ? accumulated_ + SteadyClock::elapsed(started_)
                        : accumulated_;
    }
    [[nodiscard]] bool isRunning() const noexcept { return running_; }

private:
    Duration accumulated_{};
    SteadyClock::Tick started_ = 0;
    bool running_ = false;
};

} // namespace ArtifactCore
