module;
#include <utility>
#include <QString>
#include <QDateTime>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Progress;

import Render.Farm.Types;

export namespace ArtifactCore {

struct WorkerProgressSnapshot {
    QString workerId;
    bool isLocal = false;
    int completedFrames = 0;
    int failedFrames = 0;
    int totalFrames = 0;
    int currentFrame = -1;
    WorkerState state = WorkerState::Idle;
    QDateTime lastUpdated;

    float progress() const {
        return totalFrames > 0
            ? static_cast<float>(completedFrames) / static_cast<float>(totalFrames)
            : 0.0f;
    }
};

struct FarmProgressSnapshot {
    int totalCompletedFrames = 0;
    int totalFailedFrames = 0;
    int totalFrames = 0;
    int activeWorkers = 0;
    int idleWorkers = 0;
    int totalWorkers = 0;
    float overallProgress = 0.0f;
    int estimatedRemainingMs = 0;
    QDateTime startedAt;
    QDateTime estimatedCompletion;
    std::vector<WorkerProgressSnapshot> workers;
};

class LIBRARY_DLL_API ProgressAggregator {
public:
    ProgressAggregator();

    void registerWorker(const QString& workerId, bool isLocal, int totalFrames);
    void unregisterWorker(const QString& workerId);

    void reportFrameStarted(const QString& workerId, int frame);
    void reportFrameCompleted(const QString& workerId, int frame);
    void reportFrameFailed(const QString& workerId, int frame, const QString& error);

    void reportWorkerState(const QString& workerId, WorkerState state);

    void setTotalFrames(const QString& workerId, int totalFrames);

    FarmProgressSnapshot snapshot() const;

    void reset();

private:
    struct WorkerEntry {
        QString workerId;
        bool isLocal = false;
        int completedFrames = 0;
        int failedFrames = 0;
        int totalFrames = 0;
        int currentFrame = -1;
        WorkerState state = WorkerState::Idle;
        QDateTime lastUpdated;
        std::chrono::steady_clock::time_point firstFrameTime;
        bool firstFrameSeen = false;
    };

    mutable std::mutex mutex_;
    std::unordered_map<QString, WorkerEntry> workers_;
    QDateTime startedAt_;
    bool started_ = false;
};

}

