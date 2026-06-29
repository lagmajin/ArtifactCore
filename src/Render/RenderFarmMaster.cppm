module;
#include <utility>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <tbb/tbb.h>

module Render.Farm.Master;

import Render.Farm.Types;
import Render.Farm.Checkpoint;
import Core.ThreadPool;
import NetworkRPCServer;

namespace ArtifactCore {

struct WorkerProgress {
    std::atomic<int> completed{ 0 };
    std::atomic<int> failed{ 0 };
};

struct RemoteJobSlice {
    QString workerId;
    FrameRange range;
    bool assigned = false;
    bool completed = false;
};

class RenderFarmMaster::Impl {
public:
    int workerCount_;
    std::atomic<bool> busy_{ false };
    std::atomic<bool> cancelled_{ false };

    WorkerProgress totalProgress_;
    int totalFrames_ = 0;
    RenderJobResult finalResult_;

    RetryPolicy retryPolicy_;
    CheckpointPolicy checkpointPolicy_;
    std::unique_ptr<CheckpointStore> checkpointStore_;

    RenderFarmProgressCallback onProgress_;
    std::function<void(const RenderJobResult&)> onCompleted_;

    std::vector<std::atomic<int>> frameAttempts_;
    std::mutex frameAttemptsMutex_;

    tbb::task_group farmTasks_;
    mutable std::mutex resultMutex_;

    QString currentJobId_;

    // Phase 4: Remote worker support
    bool allowRemote_ = false;
    unsigned short rpcPort_ = 0;
    bool rpcRunning_ = false;
    std::vector<RemoteJobSlice> remoteSlices_;
    std::mutex remoteMutex_;

    explicit Impl(int workerCount)
        : workerCount_(workerCount > 0 ? workerCount : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2))
        , checkpointStore_(std::make_unique<CheckpointStore>())
    {}

    std::vector<FrameRange> splitRange(const FrameRange& range, int parts) const {
        std::vector<FrameRange> subRanges;
        if (parts <= 0 || range.count() <= 0) return subRanges;

        int totalFrames = range.count();
        int baseChunk = std::max(1, totalFrames / parts);
        int remainder = totalFrames - baseChunk * parts;
        int current = range.startFrame;

        for (int i = 0; i < parts && current < range.endFrame; ++i) {
            int chunkSize = baseChunk + (i < remainder ? 1 : 0);
            int chunkEnd = std::min(current + chunkSize * range.step, range.endFrame);
            subRanges.push_back({ current, chunkEnd, range.step });
            current = chunkEnd;
        }
        return subRanges;
    }

    bool shouldRetry(int frame, int currentAttempt) const {
        if (retryPolicy_.maxAttempts <= 0) return false;
        return currentAttempt < retryPolicy_.maxAttempts;
    }

    void markFrameFailed(int frame) {
        totalProgress_.failed.fetch_add(1);
        std::lock_guard<std::mutex> lock(resultMutex_);
        finalResult_.failures.addFailure(frame, 1, "Render failed");
    }

    void saveCheckpoint() {
        if (checkpointPolicy_.mode == CheckpointPolicy::Mode::Disabled) return;
        if (currentJobId_.isEmpty()) return;

        int completed = totalProgress_.completed.load();
        if (completed <= 0) return;

        CheckpointInfo cp;
        cp.jobId = currentJobId_;
        cp.completedUpToFrame = completed;
        cp.totalFrames = totalFrames_;
        cp.failures = finalResult_.failures;
        cp.updatedAt = QDateTime::currentDateTime();
        if (cp.createdAt.isNull()) cp.createdAt = cp.updatedAt;
        checkpointStore_->save(cp);
    }

    // -- Local rendering --
    void executeLocalRange(const RenderJobRequest& request, const FrameRange& subRange,
                           std::atomic<int>& checkpointCounter) {
        for (int frame = subRange.startFrame; frame < subRange.endFrame; frame += subRange.step) {
            if (cancelled_) break;

            int attempt = 0;

            do {
                if (cancelled_) break;

                bool ok = false;
                try {
                    if (request.renderFrame) {
                        request.renderFrame(frame);
                    }
                    ok = true;
                } catch (...) {}

                if (ok) {
                    totalProgress_.completed.fetch_add(1);
                    break;
                }

                ++attempt;
                markFrameFailed(frame);

                if (!shouldRetry(frame, attempt)) {
                    finalResult_.failures.setHeld(frame, true);
                    break;
                }
                int backoff = retryPolicy_.backoffMs(attempt);
                if (backoff > 0 && !cancelled_)
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
            } while (!cancelled_);

            if (checkpointPolicy_.mode == CheckpointPolicy::Mode::EveryNFrames) {
                int c = ++checkpointCounter;
                if (c >= checkpointPolicy_.interval) {
                    saveCheckpoint();
                    checkpointCounter = 0;
                }
            }
        }
    }

    // -- Remote rendering --
    void assignRemoteSlices(const RenderJobRequest& request) {
        if (!allowRemote_) return;

        auto& rpc = NetworkPCServer::instance();
        auto workers = rpc.connectedWorkers();
        if (workers.empty()) return;

        // Filter connected workers with valid IDs
        std::vector<RemoteWorkerInfo> activeWorkers;
        for (const auto& w : workers) {
            if (!w.workerId.isEmpty() && w.connected)
                activeWorkers.push_back(w);
        }
        if (activeWorkers.empty()) return;

        // Split remaining range across remote workers
        int localWorkers = workerCount_;
        int totalParts = localWorkers + static_cast<int>(activeWorkers.size());
        auto allRanges = splitRange(request.range, totalParts);

        // Assign remote slices (last N parts go to remote)
        int remoteStart = std::max(0, static_cast<int>(allRanges.size()) - static_cast<int>(activeWorkers.size()));
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            remoteSlices_.clear();
            for (size_t i = 0; i < activeWorkers.size(); ++i) {
                int idx = remoteStart + static_cast<int>(i);
                if (idx < static_cast<int>(allRanges.size())) {
                    QJsonObject jobJson;
                    jobJson["compositionId"] = request.compositionId.toString();
                    jobJson["compositionName"] = request.compositionName;
                    jobJson["startFrame"] = allRanges[idx].startFrame;
                    jobJson["endFrame"] = allRanges[idx].endFrame;
                    jobJson["step"] = allRanges[idx].step;
                    jobJson["outputPath"] = request.outputPath;

                    rpc.sendJobAssignment(activeWorkers[i].workerId, jobJson);

                    RemoteJobSlice slice;
                    slice.workerId = activeWorkers[i].workerId;
                    slice.range = allRanges[idx];
                    slice.assigned = true;
                    remoteSlices_.push_back(slice);
                }
            }
        }
    }

    void executeJob(const RenderJobRequest& request) {
        busy_ = true;
        cancelled_ = false;

        currentJobId_ = request.jobId.isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : request.jobId;

        totalProgress_ = WorkerProgress{};
        totalFrames_ = request.range.count();
        finalResult_ = RenderJobResult{};
        finalResult_.failures = FailureManifest{};

        int total = request.range.count();
        {
            std::lock_guard<std::mutex> lock(frameAttemptsMutex_);
            frameAttempts_.clear();
            frameAttempts_.resize(total);
        }

        // Assign remote slices first
        assignRemoteSlices(request);

        // Determine local range (exclude remote slices)
        FrameRange localRange = request.range;
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            for (const auto& slice : remoteSlices_) {
                if (slice.range.startFrame <= localRange.startFrame) {
                    localRange.startFrame = std::max(localRange.startFrame, slice.range.endFrame);
                }
                if (slice.range.endFrame >= localRange.endFrame) {
                    localRange.endFrame = std::min(localRange.endFrame, slice.range.startFrame);
                }
            }
        }

        // Local rendering
        if (localRange.count() > 0) {
            auto subRanges = splitRange(localRange, workerCount_);
            std::atomic<int> checkpointCounter{ 0 };

            for (const auto& subRange : subRanges) {
                farmTasks_.run([this, request, subRange, &checkpointCounter]() {
                    executeLocalRange(request, subRange, checkpointCounter);
                });
            }
        }

        farmTasks_.wait();
        saveCheckpoint();

        // Collect remote results
        collectRemoteResults();

        if (!cancelled_) {
            RenderJobResult result;
            int c = totalProgress_.completed.load();
            int f = totalProgress_.failed.load();
            result.success = f == 0;
            result.renderedFrames = c;
            result.failedFrames = f;
            result.failures = finalResult_.failures;
            if (f > 0) {
                result.errorMessage = QString("%1 frames failed (%2 held)")
                    .arg(f).arg(result.failures.heldCount());
            }
            {
                std::lock_guard<std::mutex> lock(resultMutex_);
                finalResult_ = result;
            }
            if (onCompleted_) onCompleted_(result);
        }

        busy_ = false;
    }

    void collectRemoteResults() {
        std::lock_guard<std::mutex> lock(remoteMutex_);
        for (auto& slice : remoteSlices_) {
            if (!slice.assigned) continue;
            // In Phase 4, we assume remote workers completed.
            // Future: wait for result callback from worker.
            int frames = slice.range.count();
            totalProgress_.completed.fetch_add(frames);
            slice.completed = true;
        }
    }
};

// -- Public API --

RenderFarmMaster::RenderFarmMaster(int workerCount)
    : impl_(std::make_unique<Impl>(workerCount))
{}

RenderFarmMaster::~RenderFarmMaster() = default;

void RenderFarmMaster::setWorkerCount(int count) {
    impl_->workerCount_ = std::max(1, count);
}

int RenderFarmMaster::workerCount() const {
    return impl_->workerCount_;
}

void RenderFarmMaster::submitJob(const RenderJobRequest& request) {
    if (impl_->busy_) return;
    impl_->farmTasks_.run([this, request]() {
        impl_->executeJob(request);
    });
}

void RenderFarmMaster::cancelAll() {
    impl_->cancelled_ = true;
}

bool RenderFarmMaster::isBusy() const {
    return impl_->busy_;
}

RenderJobProgress RenderFarmMaster::overallProgress() const {
    RenderJobProgress p;
    p.completedFrames.store(impl_->totalProgress_.completed.load());
    p.failedFrames.store(impl_->totalProgress_.failed.load());
    p.totalFrames = impl_->totalFrames_;
    return p;
}

RenderJobResult RenderFarmMaster::result() const {
    std::lock_guard<std::mutex> lock(impl_->resultMutex_);
    return impl_->finalResult_;
}

void RenderFarmMaster::setOnProgress(RenderFarmProgressCallback callback) {
    impl_->onProgress_ = std::move(callback);
}

void RenderFarmMaster::setOnCompleted(std::function<void(const RenderJobResult&)> callback) {
    impl_->onCompleted_ = std::move(callback);
}

void RenderFarmMaster::setRetryPolicy(const RetryPolicy& policy) {
    impl_->retryPolicy_ = policy;
}

void RenderFarmMaster::setCheckpointPolicy(const CheckpointPolicy& policy) {
    impl_->checkpointPolicy_ = policy;
}

CheckpointStore* RenderFarmMaster::checkpointStore() const {
    return impl_->checkpointStore_.get();
}

// -- Phase 4: RPC Server --

bool RenderFarmMaster::startRpcServer(unsigned short port) {
    auto& rpc = NetworkPCServer::instance();
    if (rpc.isRunning()) return true;

    // Handle worker registration
    rpc.setOnWorkerConnected([this](const RemoteWorkerInfo&) {
        // Remote worker registered - will be used on next job
    });

    rpc.setOnWorkerDisconnected([this](const QString& workerId) {
        // Mark worker's incomplete slices for reassignment
        std::lock_guard<std::mutex> lock(impl_->remoteMutex_);
        for (auto& slice : impl_->remoteSlices_) {
            if (slice.workerId == workerId && !slice.completed) {
                slice.assigned = false;
            }
        }
    });

    rpc.setOnWorkerHeartbeat([](const QString&) {
        // Heartbeat received - worker is alive
    });

    if (port == 0) port = 9876;
    bool ok = rpc.start(port);
    if (ok) {
        impl_->rpcPort_ = rpc.port();
        impl_->rpcRunning_ = true;
    }
    return ok;
}

void RenderFarmMaster::stopRpcServer() {
    NetworkPCServer::instance().stop();
    impl_->rpcRunning_ = false;
}

bool RenderFarmMaster::isRpcServerRunning() const {
    return impl_->rpcRunning_;
}

unsigned short RenderFarmMaster::rpcServerPort() const {
    return impl_->rpcPort_;
}

int RenderFarmMaster::remoteWorkerCount() const {
    return NetworkPCServer::instance().activeWorkerCount();
}

QStringList RenderFarmMaster::remoteWorkerIds() const {
    auto workers = NetworkPCServer::instance().connectedWorkers();
    QStringList ids;
    for (const auto& w : workers) {
        if (!w.workerId.isEmpty()) ids.append(w.workerId);
    }
    return ids;
}

void RenderFarmMaster::setAllowRemoteWorkers(bool allow) {
    impl_->allowRemote_ = allow;
}

bool RenderFarmMaster::allowRemoteWorkers() const {
    return impl_->allowRemote_;
}

RenderFarmMaster& RenderFarmMaster::instance() {
    static RenderFarmMaster s_instance;
    return s_instance;
}

}
