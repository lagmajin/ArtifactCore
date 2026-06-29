module;
#include <QDebug>
#include <algorithm>

module Render.Farm.Progress;

import std;

namespace ArtifactCore {

ProgressAggregator::ProgressAggregator() = default;

void ProgressAggregator::registerWorker(const QString& workerId, bool isLocal, int totalFrames) {
    std::lock_guard lock(mutex_);
    WorkerEntry entry;
    entry.workerId = workerId;
    entry.isLocal = isLocal;
    entry.totalFrames = totalFrames;
    entry.state = WorkerState::Idle;
    entry.lastUpdated = QDateTime::currentDateTime();
    workers_[workerId] = std::move(entry);

    if (!started_) {
        started_ = true;
        startedAt_ = QDateTime::currentDateTime();
    }

    qDebug() << "[ProgressAggregator] registered worker" << workerId
             << "local=" << isLocal << "totalFrames=" << totalFrames;
}

void ProgressAggregator::unregisterWorker(const QString& workerId) {
    std::lock_guard lock(mutex_);
    workers_.erase(workerId);
    qDebug() << "[ProgressAggregator] unregistered worker" << workerId;
}

void ProgressAggregator::reportFrameStarted(const QString& workerId, int frame) {
    std::lock_guard lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return;

    it->second.currentFrame = frame;
    it->second.state = WorkerState::Rendering;
    it->second.lastUpdated = QDateTime::currentDateTime();

    if (!it->second.firstFrameSeen) {
        it->second.firstFrameSeen = true;
        it->second.firstFrameTime = std::chrono::steady_clock::now();
    }
}

void ProgressAggregator::reportFrameCompleted(const QString& workerId, int /*frame*/) {
    std::lock_guard lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return;

    it->second.completedFrames++;
    it->second.currentFrame = -1;
    it->second.lastUpdated = QDateTime::currentDateTime();
}

void ProgressAggregator::reportFrameFailed(const QString& workerId, int /*frame*/, const QString& /*error*/) {
    std::lock_guard lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return;

    it->second.failedFrames++;
    it->second.currentFrame = -1;
    it->second.lastUpdated = QDateTime::currentDateTime();
}

void ProgressAggregator::reportWorkerState(const QString& workerId, WorkerState state) {
    std::lock_guard lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return;

    it->second.state = state;
    it->second.lastUpdated = QDateTime::currentDateTime();
}

void ProgressAggregator::setTotalFrames(const QString& workerId, int totalFrames) {
    std::lock_guard lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return;

    it->second.totalFrames = totalFrames;
}

FarmProgressSnapshot ProgressAggregator::snapshot() const {
    std::lock_guard lock(mutex_);

    FarmProgressSnapshot snap;
    snap.totalFrames = 0;
    snap.totalCompletedFrames = 0;
    snap.totalFailedFrames = 0;
    snap.startedAt = startedAt_;
    snap.workers.reserve(workers_.size());

    for (const auto& [id, entry] : workers_) {
        snap.totalFrames += entry.totalFrames;
        snap.totalCompletedFrames += entry.completedFrames;
        snap.totalFailedFrames += entry.failedFrames;

        WorkerProgressSnapshot ws;
        ws.workerId = entry.workerId;
        ws.isLocal = entry.isLocal;
        ws.completedFrames = entry.completedFrames;
        ws.failedFrames = entry.failedFrames;
        ws.totalFrames = entry.totalFrames;
        ws.currentFrame = entry.currentFrame;
        ws.state = entry.state;
        ws.lastUpdated = entry.lastUpdated;
        snap.workers.push_back(std::move(ws));

        if (entry.state == WorkerState::Rendering) snap.activeWorkers++;
        else snap.idleWorkers++;
    }

    snap.totalWorkers = static_cast<int>(snap.workers.size());
    snap.overallProgress = snap.totalFrames > 0
        ? static_cast<float>(snap.totalCompletedFrames + snap.totalFailedFrames) / static_cast<float>(snap.totalFrames)
        : 0.0f;

    // ETA: based on completed work rate
    if (snap.totalCompletedFrames > 0 && snap.totalFrames > 0) {
        int doneWork = snap.totalCompletedFrames + snap.totalFailedFrames;
        int remainingWork = snap.totalFrames - doneWork;
        if (remainingWork > 0) {
            // Find earliest firstFrameTime among all workers
            auto earliest = std::chrono::steady_clock::now();
            bool found = false;
            for (const auto& [id, entry] : workers_) {
                (void)id;
                if (entry.firstFrameSeen && entry.firstFrameTime < earliest) {
                    earliest = entry.firstFrameTime;
                    found = true;
                }
            }
            if (found) {
                auto elapsed = std::chrono::steady_clock::now() - earliest;
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                if (elapsedMs > 0) {
                    double ratePerMs = static_cast<double>(doneWork) / static_cast<double>(elapsedMs);
                    if (ratePerMs > 0.0) {
                        snap.estimatedRemainingMs = static_cast<int>(static_cast<double>(remainingWork) / ratePerMs);
                        snap.estimatedCompletion = QDateTime::currentDateTime().addMSecs(snap.estimatedRemainingMs);
                    }
                }
            }
        } else {
            snap.estimatedCompletion = QDateTime::currentDateTime();
        }
    }

    return snap;
}

void ProgressAggregator::reset() {
    std::lock_guard lock(mutex_);
    workers_.clear();
    started_ = false;
}

}

