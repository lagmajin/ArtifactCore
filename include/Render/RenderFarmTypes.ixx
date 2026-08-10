module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <functional>
#include <atomic>
#include <vector>
#include <optional>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Types;

import Utils.Id;

export namespace ArtifactCore {

struct RenderFrameRange {
    int startFrame = 0;
    int endFrame = 0;
    int step = 1;

    int count() const {
        if (endFrame <= startFrame || step <= 0) return 0;
        const long long span = static_cast<long long>(endFrame) - startFrame;
        const long long count = (span + step - 1) / step;
        return count > std::numeric_limits<int>::max()
            ? std::numeric_limits<int>::max() : static_cast<int>(count);
    }

    bool contains(int frame) const {
        return frame >= startFrame && frame < endFrame;
    }

    RenderFrameRange intersect(const RenderFrameRange& other) const {
        return { std::max(startFrame, other.startFrame),
                 std::min(endFrame, other.endFrame),
                 std::max(step, other.step) };
    }
};

enum class WorkerState {
    Idle,
    Rendering,
    Completed,
    Failed,
    Cancelled,
    Hold
};

struct RenderJobRequest {
    ArtifactCore::Id compositionId;
    QString compositionName;
    RenderFrameRange range;
    QString outputPath;
    bool autoVersionOutput = false;
    bool enableAudio = false;

    // Zero keeps the farm's legacy unlimited behavior.
    int jobTimeoutMs = 0;
    int frameTimeoutMs = 0;
    int priority = 0;
    QStringList dependencies;
    QString jobPool;
    QStringList allowedWorkerIds;
    QJsonObject requiredCapabilities;
    // Optional self-contained renderer job payload for out-of-process workers.
    QJsonObject renderPayload;
    QString rendererExecutable;

    // Returns true when the frame was rendered and committed successfully.
    // A false result is a retryable frame failure.
    std::function<bool(int frame)> renderFrame;

    QString jobId;
};

struct RenderJobTemplate {
    QString name;
    RenderJobRequest request;
};

struct RenderJobProgress {
    std::atomic<int> completedFrames{ 0 };
    std::atomic<int> failedFrames{ 0 };
    int totalFrames = 0;
    qint64 elapsedMs = 0;
    qint64 estimatedRemainingMs = -1;

    RenderJobProgress() = default;

    RenderJobProgress(const RenderJobProgress& other)
        : completedFrames(other.completedFrames.load())
        , failedFrames(other.failedFrames.load())
        , totalFrames(other.totalFrames)
        , elapsedMs(other.elapsedMs)
        , estimatedRemainingMs(other.estimatedRemainingMs) {}

    RenderJobProgress& operator=(const RenderJobProgress& other) {
        if (this != &other) {
            completedFrames.store(other.completedFrames.load());
            failedFrames.store(other.failedFrames.load());
            totalFrames = other.totalFrames;
            elapsedMs = other.elapsedMs;
            estimatedRemainingMs = other.estimatedRemainingMs;
        }
        return *this;
    }

    RenderJobProgress(RenderJobProgress&& other) noexcept
        : completedFrames(other.completedFrames.load())
        , failedFrames(other.failedFrames.load())
        , totalFrames(other.totalFrames)
        , elapsedMs(other.elapsedMs)
        , estimatedRemainingMs(other.estimatedRemainingMs) {}

    RenderJobProgress& operator=(RenderJobProgress&& other) noexcept {
        if (this != &other) {
            completedFrames.store(other.completedFrames.load());
            failedFrames.store(other.failedFrames.load());
            totalFrames = other.totalFrames;
            elapsedMs = other.elapsedMs;
            estimatedRemainingMs = other.estimatedRemainingMs;
        }
        return *this;
    }

    float progress() const {
        if (totalFrames <= 0) return 0.0f;
        const double normalized = static_cast<double>(completedFrames.load()) /
                                  static_cast<double>(totalFrames);
        return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
    }

    int remainingFrames() const {
        const long long remaining = static_cast<long long>(totalFrames) -
            static_cast<long long>(completedFrames.load()) -
            static_cast<long long>(failedFrames.load());
        return static_cast<int>(std::clamp<long long>(
            remaining, 0, std::numeric_limits<int>::max()));
    }

    bool isFinished() const {
        if (totalFrames <= 0) return true;
        const long long processed = static_cast<long long>(completedFrames.load()) +
                                    static_cast<long long>(failedFrames.load());
        return processed >= static_cast<long long>(totalFrames);
    }
};

struct FailedFrameRecord {
    int frame = 0;
    int attempt = 0;
    QString errorMessage;
    bool held = false;
};

struct FailureManifest {
    std::vector<FailedFrameRecord> failedFrames;
    int totalRetries = 0;

    int heldCount() const {
        int c = 0;
        for (auto& f : failedFrames) { if (f.held) ++c; }
        return c;
    }

    bool isFrameHeld(int frame) const {
        for (auto& f : failedFrames) { if (f.frame == frame && f.held) return true; }
        return false;
    }

    void addFailure(int frame, int attempt, const QString& error) {
        for (auto& f : failedFrames) {
            if (f.frame == frame) {
                f.attempt = attempt;
                f.errorMessage = error;
                return;
            }
        }
        failedFrames.push_back({ frame, attempt, error, false });
    }

    void setHeld(int frame, bool held = true) {
        for (auto& f : failedFrames) {
            if (f.frame == frame) { f.held = held; return; }
        }
    }
};

enum class RetryBackoffStrategy {
    Linear,
    Exponential,
    Fixed
};

struct RetryPolicy {
    int maxAttempts = 3;
    int initialBackoffMs = 2000;
    int maxBackoffMs = 60000;
    double backoffFactor = 2.0;
    RetryBackoffStrategy strategy = RetryBackoffStrategy::Exponential;

    int backoffMs(int attempt) const {
        if (attempt <= 1) return 0;
        const int safeMax = std::max(0, maxBackoffMs);
        const int safeInitial = std::clamp(initialBackoffMs, 0, safeMax);
        if (safeInitial == 0 || safeMax == 0) return 0;
        const int n = attempt - 1;
        switch (strategy) {
        case RetryBackoffStrategy::Fixed:
            return safeInitial;
        case RetryBackoffStrategy::Linear: {
            const long long candidate = static_cast<long long>(safeInitial) * n;
            return static_cast<int>(std::min<long long>(candidate, safeMax));
        }
        case RetryBackoffStrategy::Exponential:
        default: {
            const long double factor =
                std::isfinite(backoffFactor) && backoffFactor > 1.0
                    ? static_cast<long double>(backoffFactor) : 1.0L;
            const long double candidate = static_cast<long double>(safeInitial) *
                std::pow(factor, static_cast<long double>(n - 1));
            if (!std::isfinite(candidate) || candidate >= safeMax) return safeMax;
            return candidate <= 0.0L ? 0 : static_cast<int>(candidate);
        }
        }
    }
};

struct CheckpointInfo {
    QString jobId;
    int completedUpToFrame = -1;
    int totalFrames = 0;
    FailureManifest failures;
    QDateTime createdAt;
    QDateTime updatedAt;
    int schemaVersion = 1;
};

struct CheckpointPolicy {
    enum class Mode { Disabled, EveryNFrames, EveryMSeconds };
    Mode mode = Mode::Disabled;
    int interval = 10;
};

struct RenderJobResult {
    bool success = false;
    int renderedFrames = 0;
    int failedFrames = 0;
    qint64 elapsedMs = 0;
    QString errorMessage;
    FailureManifest failures;
};

using RenderFarmProgressCallback = std::function<void(const RenderJobProgress&)>;
using RenderFarmAlertCallback = std::function<void(const QString&, const RenderJobResult&)>;

}
