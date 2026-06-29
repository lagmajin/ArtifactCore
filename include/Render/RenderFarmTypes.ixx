module;
#include <utility>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <atomic>
#include <vector>
#include <optional>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Types;

import Utils.Id;

export namespace ArtifactCore {

struct FrameRange {
    int startFrame = 0;
    int endFrame = 0;
    int step = 1;

    int count() const {
        if (endFrame <= startFrame || step <= 0) return 0;
        return (endFrame - startFrame + step - 1) / step;
    }

    bool contains(int frame) const {
        return frame >= startFrame && frame < endFrame;
    }

    FrameRange intersect(const FrameRange& other) const {
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
    FrameRange range;
    QString outputPath;
    bool enableAudio = false;

    std::function<void(int frame)> renderFrame;

    QString jobId;
};

struct RenderJobProgress {
    std::atomic<int> completedFrames{ 0 };
    std::atomic<int> failedFrames{ 0 };
    int totalFrames = 0;

    float progress() const {
        return totalFrames > 0
            ? static_cast<float>(completedFrames) / static_cast<float>(totalFrames)
            : 0.0f;
    }

    int remainingFrames() const {
        return std::max(0, totalFrames - completedFrames.load() - failedFrames.load());
    }

    bool isFinished() const {
        return completedFrames + failedFrames >= totalFrames;
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
        int n = attempt - 1;
        switch (strategy) {
        case RetryBackoffStrategy::Fixed:
            return std::min(initialBackoffMs, maxBackoffMs);
        case RetryBackoffStrategy::Linear:
            return std::min(initialBackoffMs * n, maxBackoffMs);
        case RetryBackoffStrategy::Exponential:
        default: {
            int ms = static_cast<int>(initialBackoffMs * std::pow(backoffFactor, n - 1));
            return std::min(ms, maxBackoffMs);
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
    QString errorMessage;
    FailureManifest failures;
};

using RenderFarmProgressCallback = std::function<void(const RenderJobProgress&)>;

}
