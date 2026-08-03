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
#include <unordered_set>
#include <map>
#include <deque>
#include <string>
#include <condition_variable>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#ifdef emit
#undef emit
#endif

module Render.Farm.Master;

import Render.Farm.Types;
import Render.Farm.Checkpoint;
import Core.ThreadPool;
import NetworkRPCServer;

namespace ArtifactCore {

struct WorkerProgress {
    std::atomic<int> completed{ 0 };
    std::atomic<int> failed{ 0 };

    WorkerProgress() = default;

    WorkerProgress(const WorkerProgress& other)
        : completed(other.completed.load())
        , failed(other.failed.load()) {}

    WorkerProgress& operator=(const WorkerProgress& other) {
        if (this != &other) {
            completed.store(other.completed.load());
            failed.store(other.failed.load());
        }
        return *this;
    }

    WorkerProgress(WorkerProgress&& other) noexcept
        : completed(other.completed.load())
        , failed(other.failed.load()) {}

    WorkerProgress& operator=(WorkerProgress&& other) noexcept {
        if (this != &other) {
            completed.store(other.completed.load());
            failed.store(other.failed.load());
        }
        return *this;
    }
};

struct RemoteJobSlice {
    QString workerId;
    RenderFrameRange range;
    bool assigned = false;
    bool completed = false;
    int framesCompleted_ = 0;  // how many individual frames reported done
    std::unordered_set<int> reportedFrames;
};

class RenderFarmMaster::Impl {
public:
    int workerCount_;
    std::atomic<bool> busy_{ false };
    std::atomic<bool> cancelled_{ false };
    std::atomic<bool> paused_{ false };
    std::atomic<bool> timedOut_{ false };
    std::mutex pauseMutex_;
    std::condition_variable pauseCv_;

    WorkerProgress totalProgress_;
    int totalFrames_ = 0;
    RenderJobResult finalResult_;

    RetryPolicy retryPolicy_;
    CheckpointPolicy checkpointPolicy_;
    std::unique_ptr<CheckpointStore> checkpointStore_;

    RenderFarmProgressCallback onProgress_;
    std::function<void(const RenderJobResult&)> onCompleted_;
    RenderFarmAlertCallback onAlert_;
    double failureAlertThreshold_ = 0.0;

    std::vector<int> frameAttempts_;
    std::mutex frameAttemptsMutex_;

    std::thread farmThread_;
    std::deque<RenderJobRequest> pendingJobs_;
    mutable std::mutex pendingJobsMutex_;
    mutable std::mutex resultMutex_;
    std::chrono::steady_clock::time_point jobDeadline_{};
    std::chrono::steady_clock::time_point jobStartedAt_{};
    std::atomic<qint64> lastElapsedMs_{ 0 };
    bool hasJobDeadline_ = false;

    QString currentJobId_;
    ArtifactCore::Id currentCompositionId_;

    // Phase 4: Remote worker support
    bool allowRemote_ = false;
    unsigned short rpcPort_ = 0;
    bool rpcRunning_ = false;
    std::vector<RemoteJobSlice> remoteSlices_;
    std::mutex remoteMutex_;
    std::atomic<int> remoteCompleted_{0};
    int totalRemoteFrames_ = 0;
    std::mutex remoteWaitMutex_;
    std::condition_variable remoteCv_;
    std::function<void(const QString&, int, bool)> onRemoteFrameResult_;
    std::map<QString, RenderJobRequest> jobTemplates_;
    mutable std::mutex jobTemplatesMutex_;
    std::map<QString, RenderJobRequest> jobHistory_;
    std::deque<QString> jobHistoryOrder_;
    mutable std::mutex jobHistoryMutex_;

    void emitProgress() {
        if (!onProgress_) return;
        RenderJobProgress progress;
        progress.completedFrames.store(totalProgress_.completed.load());
        progress.failedFrames.store(totalProgress_.failed.load());
        progress.totalFrames = totalFrames_;
        progress.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - jobStartedAt_).count();
        const int processed = progress.completedFrames.load() +
            progress.failedFrames.load();
        if (processed > 0 && totalFrames_ > processed) {
            progress.estimatedRemainingMs = static_cast<qint64>(
                (static_cast<double>(progress.elapsedMs) / processed) *
                (totalFrames_ - processed));
        } else if (totalFrames_ <= processed) {
            progress.estimatedRemainingMs = 0;
        }
        onProgress_(progress);
    }

    void notifyCompleted(const RenderJobResult& result) {
        if (onCompleted_) onCompleted_(result);
        if (onAlert_ && failureAlertThreshold_ > 0.0 && totalFrames_ > 0) {
            const double failureFraction = static_cast<double>(result.failedFrames)
                / static_cast<double>(totalFrames_);
            if (failureFraction >= failureAlertThreshold_)
                onAlert_(QStringLiteral("failure_rate"), result);
        }
    }

    explicit Impl(int workerCount)
        : workerCount_(workerCount > 0 ? workerCount : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2))
        , checkpointStore_(std::make_unique<CheckpointStore>())
    {}

    ~Impl() {
        if (farmThread_.joinable()) {
            farmThread_.join();
        }
    }

    std::vector<RenderFrameRange> splitRange(const RenderFrameRange& range, int parts) const {
        std::vector<RenderFrameRange> subRanges;
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

    bool workerMatches(const RemoteWorkerInfo& worker, const QJsonObject& requirements) const {
        for (auto it = requirements.constBegin(); it != requirements.constEnd(); ++it) {
            const QJsonValue actual = worker.capabilities.value(it.key());
            if (actual.isString() && it.value().isString()) {
                if (actual.toString().compare(it.value().toString(), Qt::CaseInsensitive) != 0)
                    return false;
            } else if (actual.isArray() && it.value().isArray()) {
                const QJsonArray available = actual.toArray();
                for (const auto& required : it.value().toArray()) {
                    bool found = false;
                    for (const auto& candidate : available) {
                        if (candidate.isString() && required.isString()
                            && candidate.toString().compare(required.toString(), Qt::CaseInsensitive) == 0) {
                            found = true;
                            break;
                        }
                        if (candidate == required) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) return false;
                }
            } else if (actual.isArray() && it.value().isString()) {
                bool found = false;
                for (const auto& candidate : actual.toArray()) {
                    if (candidate.isString()
                        && candidate.toString().compare(it.value().toString(), Qt::CaseInsensitive) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            } else if (actual.isObject() && it.value().isObject()) {
                const QJsonObject available = actual.toObject();
                const QJsonObject required = it.value().toObject();
                for (auto requiredIt = required.constBegin();
                     requiredIt != required.constEnd(); ++requiredIt) {
                    const QJsonValue candidate = available.value(requiredIt.key());
                    if (candidate.isString() && requiredIt.value().isString()) {
                        if (candidate.toString().compare(requiredIt.value().toString(), Qt::CaseInsensitive) != 0)
                            return false;
                    } else if (candidate != requiredIt.value()) {
                        return false;
                    }
                }
            } else if (actual.isDouble() && it.value().isDouble()) {
                if (actual.toDouble() < it.value().toDouble()) return false;
            } else if (actual != it.value()) {
                return false;
            }
        }
        return true;
    }

    QString validateOutputPath(const QString& outputPath) const {
        const QString path = outputPath.trimmed();
        if (path.isEmpty()) return {};
        const QFileInfo info(path);
        const QDir parent = info.dir();
        if (!parent.exists())
            return QStringLiteral("Output directory does not exist: %1").arg(parent.path());
        const QFileInfo parentInfo(parent.path());
        if (!parent.isReadable() || !parentInfo.isWritable())
            return QStringLiteral("Output directory is not accessible: %1").arg(parent.path());
        if (info.exists() && !info.isWritable())
            return QStringLiteral("Output file is not writable: %1").arg(path);
        return {};
    }

    QString validateOutputArtifact(const QString& outputPath,
                                   const RenderFrameRange& range) const {
        const QString path = outputPath.trimmed();
        if (path.isEmpty()) return {};
        const bool hashSequence = path.contains(QStringLiteral("####"));
        int printfWidth = 0;
        int printfTokenStart = path.indexOf(QStringLiteral("%0"));
        if (printfTokenStart >= 0) {
            const int d = path.indexOf(QChar('d'), printfTokenStart + 2);
            if (d > printfTokenStart + 2) {
                bool ok = false;
                printfWidth = path.mid(printfTokenStart + 2, d - printfTokenStart - 2).toInt(&ok);
                if (!ok || printfWidth <= 0) printfWidth = 0;
            }
        }
        if (hashSequence || printfWidth > 0) {
            for (int frame = range.startFrame; frame < range.endFrame; frame += range.step) {
                QString framePath = path;
                if (hashSequence) {
                    framePath.replace(QStringLiteral("####"),
                                      QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
                } else {
                    const QString token = path.mid(printfTokenStart, path.indexOf(QChar('d'), printfTokenStart) - printfTokenStart + 1);
                    framePath.replace(token,
                                      QStringLiteral("%1").arg(frame, printfWidth, 10, QChar('0')));
                }
                const QFileInfo frameInfo(framePath);
                if (!frameInfo.isFile() || frameInfo.size() <= 0)
                    return QStringLiteral("Missing or empty render output for frame %1: %2")
                        .arg(frame).arg(framePath);
            }
            return {};
        }
        if (path.contains('%') || path.contains('*')) return {};
        const QFileInfo info(path);
        if (!info.exists())
            return QStringLiteral("Render output was not created: %1").arg(path);
        if (!info.isFile() || info.size() <= 0)
            return QStringLiteral("Render output is empty or not a file: %1").arg(path);
        return {};
    }

    void recordFrameFailure(int frame) {
        std::lock_guard<std::mutex> lock(resultMutex_);
        finalResult_.failures.addFailure(frame, 1, "Render failed");
    }

    void markFrameFailed(int frame) {
        totalProgress_.failed.fetch_add(1);
        recordFrameFailure(frame);
    }

    void saveCheckpoint(int baseFrame = 0) {
        if (checkpointPolicy_.mode == CheckpointPolicy::Mode::Disabled) return;
        if (currentJobId_.isEmpty()) return;

        int completed = totalProgress_.completed.load();
        if (completed <= 0) return;

        CheckpointInfo cp;
        cp.jobId = currentJobId_;
        cp.completedUpToFrame = baseFrame + completed;  // absolute frame (exclusive)
        cp.totalFrames = totalFrames_;
        cp.failures = finalResult_.failures;
        cp.updatedAt = QDateTime::currentDateTime();
        if (cp.createdAt.isNull()) cp.createdAt = cp.updatedAt;
        checkpointStore_->save(cp);
    }

    // -- Local rendering --
    void executeLocalRange(const RenderJobRequest& request, const RenderFrameRange& subRange,
                           std::atomic<int>& checkpointCounter) {
        for (int frame = subRange.startFrame; frame < subRange.endFrame; frame += subRange.step) {
            {
                std::unique_lock<std::mutex> lock(pauseMutex_);
                pauseCv_.wait(lock, [this]() { return !paused_.load() || cancelled_.load(); });
            }
            if (cancelled_ || (hasJobDeadline_ && std::chrono::steady_clock::now() >= jobDeadline_)) {
                if (hasJobDeadline_ && std::chrono::steady_clock::now() >= jobDeadline_)
                    timedOut_ = true;
                cancelled_ = true;
                break;
            }

            int attempt = 0;

            do {
                {
                    std::unique_lock<std::mutex> lock(pauseMutex_);
                    pauseCv_.wait(lock, [this]() { return !paused_.load() || cancelled_.load(); });
                }
                if (cancelled_ || (hasJobDeadline_ && std::chrono::steady_clock::now() >= jobDeadline_)) {
                    if (hasJobDeadline_ && std::chrono::steady_clock::now() >= jobDeadline_)
                        timedOut_ = true;
                    cancelled_ = true;
                    break;
                }

                bool ok = false;
                const auto frameStart = std::chrono::steady_clock::now();
                try {
                    if (request.renderFrame) {
                        ok = request.renderFrame(frame);
                    }
                } catch (...) {}

                if (ok && request.frameTimeoutMs > 0) {
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - frameStart).count();
                    ok = elapsed <= request.frameTimeoutMs;
                }

                if (ok) {
                    totalProgress_.completed.fetch_add(1);
                    emitProgress();
                    break;
                }

                ++attempt;
                recordFrameFailure(frame);

                if (!shouldRetry(frame, attempt)) {
                    totalProgress_.failed.fetch_add(1);
                    finalResult_.failures.setHeld(frame, true);
                    emitProgress();
                    break;
                }
                int backoff = retryPolicy_.backoffMs(attempt);
                if (backoff > 0 && !cancelled_)
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
            } while (!cancelled_);

            if (checkpointPolicy_.mode == CheckpointPolicy::Mode::EveryNFrames) {
                int c = ++checkpointCounter;
                if (c >= checkpointPolicy_.interval) {
                    saveCheckpoint(request.range.startFrame);
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
            if (!w.workerId.isEmpty() && w.connected && w.assignedFrames == 0
                && w.state == QStringLiteral("Idle")
                && workerMatches(w, request.requiredCapabilities))
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
                    jobJson["priority"] = request.priority;
                    jobJson["jobTimeoutMs"] = request.jobTimeoutMs;
                    jobJson["frameTimeoutMs"] = request.frameTimeoutMs;
                    jobJson["retryMaxAttempts"] = retryPolicy_.maxAttempts;
                    jobJson["retryInitialBackoffMs"] = retryPolicy_.initialBackoffMs;
                    if (!request.renderPayload.isEmpty())
                        jobJson["renderPayload"] = request.renderPayload;
                    if (!request.rendererExecutable.isEmpty())
                        jobJson["rendererExecutable"] = request.rendererExecutable;

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
        paused_ = false;
        timedOut_ = false;
        jobStartedAt_ = std::chrono::steady_clock::now();

        const QString outputError = validateOutputPath(request.outputPath);
        if (!outputError.isEmpty()) {
            RenderJobResult result;
            result.errorMessage = outputError;
            {
                std::lock_guard<std::mutex> lock(resultMutex_);
                finalResult_ = result;
            }
            notifyCompleted(result);
            lastElapsedMs_.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - jobStartedAt_).count());
            busy_ = false;
            return;
        }

        hasJobDeadline_ = request.jobTimeoutMs > 0;
        if (hasJobDeadline_) {
            jobDeadline_ = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(request.jobTimeoutMs);
        }

        // Clear remote state from previous job
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            remoteSlices_.clear();
        }
        remoteCompleted_ = 0;
        totalRemoteFrames_ = 0;

        currentJobId_ = request.jobId.isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : request.jobId;
        currentCompositionId_ = request.compositionId;

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

        // Checkpoint restore: pick up from last completed frame
        int restoreUpTo = -1;
        if (checkpointPolicy_.mode != CheckpointPolicy::Mode::Disabled && !currentJobId_.isEmpty()) {
            auto existing = checkpointStore_->load(currentJobId_);
            if (existing) {
                restoreUpTo = std::min(existing->completedUpToFrame, request.range.endFrame);
                int alreadyDone = std::max(0, restoreUpTo - request.range.startFrame);
                totalProgress_.completed.store(alreadyDone);
                totalFrames_ = std::max(totalFrames_, existing->totalFrames);
                finalResult_.failures = existing->failures;
            }
        }

        // Assign remote slices first
        assignRemoteSlices(request);

        // Determine local ranges by subtracting every remote slice. Do not
        // assume that remote slices are adjacent or that one contiguous local
        // range remains after remote assignment.
        std::vector<RenderFrameRange> localRanges{ request.range };
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            for (const auto& slice : remoteSlices_) {
                std::vector<RenderFrameRange> remaining;
                for (const auto& candidate : localRanges) {
                    const int overlapStart = std::max(candidate.startFrame, slice.range.startFrame);
                    const int overlapEnd = std::min(candidate.endFrame, slice.range.endFrame);
                    if (overlapStart >= overlapEnd) {
                        remaining.push_back(candidate);
                        continue;
                    }
                    if (candidate.startFrame < overlapStart)
                        remaining.push_back({ candidate.startFrame, overlapStart, candidate.step });
                    if (overlapEnd < candidate.endFrame)
                        remaining.push_back({ overlapEnd, candidate.endFrame, candidate.step });
                }
                localRanges = std::move(remaining);
            }
        }

        std::atomic<int> checkpointCounter{ 0 };
        std::vector<std::thread> workers;
        for (auto localRange : localRanges) {
            if (restoreUpTo > 0 && restoreUpTo > localRange.startFrame)
                localRange.startFrame = std::min(restoreUpTo, localRange.endFrame);
            if (localRange.count() <= 0) continue;
            auto subRanges = splitRange(localRange, workerCount_);
            workers.reserve(workers.size() + subRanges.size());
            for (const auto& subRange : subRanges) {
                workers.emplace_back([this, request, subRange, &checkpointCounter]() {
                    executeLocalRange(request, subRange, checkpointCounter);
                });
            }
        }
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        // Collect remote results before checkpoint so the checkpoint includes remote progress
        collectRemoteResults();
        saveCheckpoint(request.range.startFrame);

        if (!cancelled_ && totalProgress_.failed.load() == 0) {
            const QString outputError = validateOutputArtifact(request.outputPath, request.range);
            if (!outputError.isEmpty()) {
                std::lock_guard<std::mutex> lock(resultMutex_);
                finalResult_.errorMessage = outputError;
                finalResult_.success = false;
            }
        }

        if (!cancelled_ || timedOut_) {
            RenderJobResult result;
            int c = totalProgress_.completed.load();
            int f = totalProgress_.failed.load();
            result.success = f == 0;
            result.renderedFrames = c;
            result.failedFrames = f;
            result.failures = finalResult_.failures;
            if (!finalResult_.errorMessage.isEmpty()) {
                result.success = false;
                result.errorMessage = finalResult_.errorMessage;
            }
            if (f > 0) {
                result.errorMessage = QString("%1 frames failed (%2 held)")
                    .arg(f).arg(result.failures.heldCount());
            }
            if (timedOut_) {
                result.success = false;
                result.errorMessage = QStringLiteral("Render farm job timed out");
            }
            {
                std::lock_guard<std::mutex> lock(resultMutex_);
                finalResult_ = result;
            }
            notifyCompleted(result);
        }

        lastElapsedMs_.store(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - jobStartedAt_).count());
        busy_ = false;
    }

    void collectRemoteResults() {
        if (remoteSlices_.empty()) return;

        int totalRemote = 0;
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            for (const auto& slice : remoteSlices_)
                totalRemote += slice.range.count();
        }
        totalRemoteFrames_ = totalRemote;
        remoteCompleted_ = 0;

        const auto waitDuration = hasJobDeadline_
            ? std::chrono::duration_cast<std::chrono::milliseconds>(
                std::max(std::chrono::steady_clock::duration::zero(), jobDeadline_
                    - std::chrono::steady_clock::now()))
            : std::chrono::minutes(10);
        const auto markUnreportedFrames = [this](const RemoteJobSlice& slice) {
            for (int frame = slice.range.startFrame; frame < slice.range.endFrame;
                 frame += slice.range.step) {
                if (!slice.reportedFrames.contains(frame))
                    markFrameFailed(frame);
            }
        };
        {
            std::unique_lock<std::mutex> waitLock(remoteWaitMutex_);
            if (!remoteCv_.wait_for(waitLock,
                    waitDuration,
                    [this]() {
                        return remoteCompleted_.load() >= totalRemoteFrames_ ||
                               cancelled_.load();
                    })) {
                // Timeout: mark all incomplete as failures (skip already-reported)
                std::lock_guard<std::mutex> lock(remoteMutex_);
                for (auto& slice : remoteSlices_) {
                    markUnreportedFrames(slice);
                }
                return;
            }
        }

        // Mark any uncompleted remote frames as failures.
        // Frames already reported via onRemoteFrameResult_ are already counted
        // in totalProgress_.completed, so we only charge the remainder.
        int failCount = 0;
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            for (const auto& slice : remoteSlices_) {
                int remaining = slice.range.count() - static_cast<int>(slice.reportedFrames.size());
                if (remaining > 0) {
                    markUnreportedFrames(slice);
                    failCount += remaining;
                }
            }
        }
        if (failCount > 0) {
            finalResult_.errorMessage += QStringLiteral("; %1 remote frames failed").arg(failCount);
        }
    }
};

// -- Public API --

RenderFarmMaster::RenderFarmMaster(int workerCount)
    : impl_(std::make_unique<Impl>(workerCount))
{}

RenderFarmMaster::~RenderFarmMaster() {
    if (isHttpApiRunning()) stopHttpApi();
}

void RenderFarmMaster::setWorkerCount(int count) {
    impl_->workerCount_ = std::max(1, count);
}

int RenderFarmMaster::workerCount() const {
    return impl_->workerCount_;
}

void RenderFarmMaster::submitJob(const RenderJobRequest& request) {
    RenderJobRequest tracked = request;
    if (tracked.jobId.isEmpty())
        tracked.jobId = QStringLiteral("farm-%1").arg(QDateTime::currentMSecsSinceEpoch());
    {
        std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
        const auto existing = impl_->jobHistory_.find(tracked.jobId);
        if (existing == impl_->jobHistory_.end())
            impl_->jobHistoryOrder_.push_back(tracked.jobId);
        impl_->jobHistory_[tracked.jobId] = tracked;
        while (impl_->jobHistoryOrder_.size() > 512) {
            const QString oldest = impl_->jobHistoryOrder_.front();
            impl_->jobHistoryOrder_.pop_front();
            impl_->jobHistory_.erase(oldest);
        }
    }
    if (impl_->busy_) {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        impl_->pendingJobs_.push_back(tracked);
        return;
    }
    if (impl_->farmThread_.joinable()) {
        impl_->farmThread_.join();
    }
    impl_->farmThread_ = std::thread([this, tracked]() {
        impl_->executeJob(tracked);
        while (true) {
            RenderJobRequest next;
            {
                std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
                if (impl_->pendingJobs_.empty()) break;
                next = impl_->pendingJobs_.front();
                impl_->pendingJobs_.pop_front();
            }
            impl_->executeJob(next);
        }
    });
}

bool RenderFarmMaster::resubmitJob(const QString& jobId) {
    RenderJobRequest request;
    {
        std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
        const auto it = impl_->jobHistory_.find(jobId.trimmed());
        if (it == impl_->jobHistory_.end()) return false;
        request = it->second;
    }
    request.jobId = QStringLiteral("%1-retry-%2")
        .arg(jobId.trimmed()).arg(QDateTime::currentMSecsSinceEpoch());
    submitJob(request);
    return true;
}

QStringList RenderFarmMaster::jobHistory() const {
    QStringList ids;
    std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
    for (const auto& [jobId, request] : impl_->jobHistory_)
        ids.push_back(jobId);
    return ids;
}

bool RenderFarmMaster::submitTemplate(const QString& name) {
    const auto request = jobTemplate(name);
    if (!request) return false;
    RenderJobRequest submitted = *request;
    if (submitted.jobId.isEmpty()) {
        submitted.jobId = QStringLiteral("template-%1-%2")
            .arg(name.trimmed()).arg(QDateTime::currentMSecsSinceEpoch());
    }
    submitJob(submitted);
    return true;
}

bool RenderFarmMaster::registerJobTemplate(const RenderJobTemplate& jobTemplate) {
    const QString name = jobTemplate.name.trimmed();
    if (name.isEmpty() || jobTemplate.request.range.count() <= 0)
        return false;
    std::lock_guard<std::mutex> lock(impl_->jobTemplatesMutex_);
    impl_->jobTemplates_[name] = jobTemplate.request;
    return true;
}

bool RenderFarmMaster::removeJobTemplate(const QString& name) {
    std::lock_guard<std::mutex> lock(impl_->jobTemplatesMutex_);
    return impl_->jobTemplates_.erase(name.trimmed()) > 0;
}

QStringList RenderFarmMaster::jobTemplateNames() const {
    QStringList names;
    std::lock_guard<std::mutex> lock(impl_->jobTemplatesMutex_);
    for (const auto& [name, request] : impl_->jobTemplates_)
        names.push_back(name);
    return names;
}

std::optional<RenderJobRequest> RenderFarmMaster::jobTemplate(const QString& name) const {
    std::lock_guard<std::mutex> lock(impl_->jobTemplatesMutex_);
    const auto it = impl_->jobTemplates_.find(name.trimmed());
    if (it == impl_->jobTemplates_.end()) return std::nullopt;
    return it->second;
}

void RenderFarmMaster::cancelAll() {
    impl_->cancelled_ = true;
    {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        impl_->pendingJobs_.clear();
    }
    impl_->pauseCv_.notify_all();
}

void RenderFarmMaster::pause() {
    if (impl_->busy_) impl_->paused_ = true;
}

void RenderFarmMaster::resume() {
    impl_->paused_ = false;
    impl_->pauseCv_.notify_all();
}

bool RenderFarmMaster::isPaused() const {
    return impl_->paused_.load();
}

bool RenderFarmMaster::isBusy() const {
    return impl_->busy_;
}

RenderJobProgress RenderFarmMaster::overallProgress() const {
    RenderJobProgress p;
    p.completedFrames.store(impl_->totalProgress_.completed.load());
    p.failedFrames.store(impl_->totalProgress_.failed.load());
    p.totalFrames = impl_->totalFrames_;
    if (impl_->busy_) {
        p.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - impl_->jobStartedAt_).count();
    } else {
        p.elapsedMs = impl_->lastElapsedMs_.load();
    }
    const int completed = p.completedFrames.load();
    const int failed = p.failedFrames.load();
    const int processed = completed + failed;
    if (processed > 0 && p.totalFrames > processed && p.elapsedMs > 0) {
        const double perFrameMs = static_cast<double>(p.elapsedMs) / processed;
        p.estimatedRemainingMs = static_cast<qint64>(
            perFrameMs * (p.totalFrames - processed));
    } else if (p.totalFrames <= processed) {
        p.estimatedRemainingMs = 0;
    }
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

void RenderFarmMaster::setOnAlert(RenderFarmAlertCallback callback) {
    impl_->onAlert_ = std::move(callback);
}

void RenderFarmMaster::setFailureAlertThreshold(double fraction) {
    impl_->failureAlertThreshold_ = std::clamp(fraction, 0.0, 1.0);
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
        // Unblock collectRemoteResults by accounting for remaining frames from the dead worker.
        // collectRemoteResults will mark incomplete slices as failures.
        std::lock_guard<std::mutex> lock(impl_->remoteMutex_);
        for (auto& slice : impl_->remoteSlices_) {
            if (slice.workerId == workerId && !slice.completed) {
                int remaining = slice.range.count() - slice.framesCompleted_;
                if (remaining > 0)
                    impl_->remoteCompleted_.fetch_add(remaining);
            }
        }
        impl_->remoteCv_.notify_all();
    });

    rpc.setOnWorkerHeartbeat([](const QString&) {
        // Heartbeat received - worker is alive
    });

    // Handle incoming RPC from workers: frameCompleted / frameFailed
    impl_->onRemoteFrameResult_ = [this](const QString& workerId, int frame, bool success) {
        {
            std::lock_guard<std::mutex> lock(impl_->remoteMutex_);
            bool accepted = false;
            for (auto& slice : impl_->remoteSlices_) {
                if (slice.workerId == workerId &&
                    frame >= slice.range.startFrame &&
                    frame < slice.range.endFrame &&
                    ((frame - slice.range.startFrame) % slice.range.step) == 0) {
                    accepted = slice.reportedFrames.insert(frame).second;
                    if (accepted) {
                        ++slice.framesCompleted_;
                        if (slice.framesCompleted_ >= slice.range.count())
                            slice.completed = true;
                    }
                    break;
                }
            }
            if (!accepted) return;
        }
        if (success) {
            impl_->totalProgress_.completed.fetch_add(1);
        } else {
            impl_->markFrameFailed(frame);
        }
        impl_->remoteCompleted_.fetch_add(1);
        impl_->emitProgress();
        // Mark the slice that contains this frame as completed
        impl_->remoteCv_.notify_all();
    };

    rpc.setOnRequest([this](const QString& method, const QJsonObject& params) -> QJsonObject {
        if (method == QStringLiteral("submitJob")) {
            RenderJobRequest request;
            request.jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            if (request.jobId.isEmpty()) {
                request.jobId = QStringLiteral("farm-http-%1")
                    .arg(QDateTime::currentMSecsSinceEpoch());
            }
            request.compositionName = params.value(QStringLiteral("compositionName")).toString();
            const QString compositionId = params.value(QStringLiteral("compositionId")).toString().trimmed();
            if (!compositionId.isEmpty()) request.compositionId = ArtifactCore::Id(compositionId);
            request.range.startFrame = params.value(QStringLiteral("startFrame")).toInt(0);
            request.range.endFrame = params.value(QStringLiteral("endFrame")).toInt(0);
            request.range.step = std::max(1, params.value(QStringLiteral("step")).toInt(1));
            request.priority = params.value(QStringLiteral("priority")).toInt(0);
            request.jobTimeoutMs = std::max(0, params.value(QStringLiteral("jobTimeoutMs")).toInt(0));
            request.frameTimeoutMs = std::max(0, params.value(QStringLiteral("frameTimeoutMs")).toInt(0));
            request.outputPath = params.value(QStringLiteral("outputPath")).toString();
            request.rendererExecutable = params.value(QStringLiteral("rendererExecutable")).toString();
            request.renderPayload = params.value(QStringLiteral("renderPayload")).toObject();
            request.requiredCapabilities = params.value(QStringLiteral("requiredCapabilities")).toObject();
            const QString workerPool = params.value(QStringLiteral("workerPool")).toString().trimmed();
            if (!workerPool.isEmpty()) {
                request.requiredCapabilities[QStringLiteral("pool")] = workerPool;
            }
            if (request.range.endFrame <= request.range.startFrame
                || request.renderPayload.isEmpty() || request.rendererExecutable.isEmpty()) {
                return {{QStringLiteral("status"), QStringLiteral("invalid_request")}};
            }
            if ((request.rendererExecutable.contains('/') || request.rendererExecutable.contains('\\'))
                && !QFileInfo(request.rendererExecutable).isFile()) {
                return {{QStringLiteral("status"), QStringLiteral("renderer_not_found")}};
            }
            if (!allowRemoteWorkers()) {
                return {{QStringLiteral("status"), QStringLiteral("remote_disabled")}};
            }
            if (remoteWorkerCount() <= 0) {
                return {{QStringLiteral("status"), QStringLiteral("no_workers")}};
            }
            const bool queued = isBusy();
            submitJob(request);
            return {{QStringLiteral("status"), QStringLiteral("accepted")},
                    {QStringLiteral("jobId"), request.jobId},
                    {QStringLiteral("queued"), queued}};
        }
        if (method == QStringLiteral("submitTemplate")) {
            const QString name = params.value(QStringLiteral("name")).toString().trimmed();
            const auto request = jobTemplate(name);
            if (!request)
                return {{QStringLiteral("status"), QStringLiteral("template_not_found")}};
            if (!submitTemplate(name))
                return {{QStringLiteral("status"), QStringLiteral("submit_failed")}};
            return {{QStringLiteral("status"), QStringLiteral("accepted")}};
        }
        if (method == QStringLiteral("resubmitJob")) {
            const QString jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            if (!jobHistory().contains(jobId))
                return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            resubmitJob(jobId);
            return {{QStringLiteral("status"), QStringLiteral("accepted")}};
        }
        if (method == QStringLiteral("cancelJob")) {
            cancelAll();
            return {{QStringLiteral("status"), QStringLiteral("cancel_requested")}};
        }
        if (method == QStringLiteral("pauseJob")) {
            if (!isBusy()) return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            pause();
            return {{QStringLiteral("status"), QStringLiteral("pause_requested")},
                    {QStringLiteral("jobId"), impl_->currentJobId_}};
        }
        if (method == QStringLiteral("resumeJob")) {
            if (!isBusy()) return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            resume();
            return {{QStringLiteral("status"), QStringLiteral("resume_requested")},
                    {QStringLiteral("jobId"), impl_->currentJobId_}};
        }
        if (method == QStringLiteral("frameCompleted")) {
            QString workerId = params["workerId"].toString();
            int frame = params["frame"].toInt(-1);
            if (impl_->onRemoteFrameResult_ && frame >= 0)
                impl_->onRemoteFrameResult_(workerId, frame, true);
            return {{"status", "ok"}};
        }
        if (method == QStringLiteral("frameFailed")) {
            QString workerId = params["workerId"].toString();
            int frame = params["frame"].toInt(-1);
            if (impl_->onRemoteFrameResult_ && frame >= 0)
                impl_->onRemoteFrameResult_(workerId, frame, false);
            return {{"status", "ok"}};
        }
        return {{"status", "unknown_method"}};
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

QJsonArray RenderFarmMaster::remoteWorkerSnapshot() const {
    QJsonArray snapshot;
    for (const auto& worker : NetworkPCServer::instance().connectedWorkers()) {
        snapshot.append(QJsonObject{
            {QStringLiteral("workerId"), worker.workerId},
            {QStringLiteral("address"), worker.address},
            {QStringLiteral("state"), worker.state},
            {QStringLiteral("assignedFrames"), worker.assignedFrames},
            {QStringLiteral("completedFrames"), worker.completedFrames},
            {QStringLiteral("failedFrames"), worker.failedFrames},
            {QStringLiteral("renderTimeMs"), worker.renderTimeMs},
            {QStringLiteral("totalRenderTimeMs"), worker.totalRenderTimeMs},
            {QStringLiteral("currentFrame"), worker.currentFrame},
            {QStringLiteral("lastHeartbeat"), worker.lastHeartbeat},
            {QStringLiteral("capabilities"), worker.capabilities}
        });
    }
    return snapshot;
}

void RenderFarmMaster::setAllowRemoteWorkers(bool allow) {
    impl_->allowRemote_ = allow;
}

void RenderFarmMaster::setRpcAuthToken(const QString& token) {
    NetworkPCServer::instance().setAuthToken(token);
}

bool RenderFarmMaster::setRpcTlsCertificateFiles(
    const QString& certificateFile, const QString& privateKeyFile) {
    return NetworkPCServer::instance().setTlsCertificateFiles(
        certificateFile, privateKeyFile);
}

bool RenderFarmMaster::setRemoteWorkerMaintenance(const QString& workerId,
                                                  bool maintenance) {
    return NetworkPCServer::instance().setWorkerMaintenance(workerId, maintenance);
}

bool RenderFarmMaster::startHttpApi(unsigned short port) {
    NetworkPCServer::instance().setHttpStatusProvider([this]() {
        const auto progress = overallProgress();
        const auto result = this->result();
        const bool busy = isBusy();
        int queuedJobs = 0;
        {
            std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
            queuedJobs = static_cast<int>(impl_->pendingJobs_.size());
        }
        const bool paused = isPaused();
        const qint64 estimatedCostMs = progress.completedFrames.load() > 0
            ? static_cast<qint64>(
                (static_cast<double>(progress.elapsedMs) / progress.completedFrames.load())
                * progress.totalFrames)
            : -1;
        const QString status = paused ? QStringLiteral("paused")
            : busy ? QStringLiteral("running")
            : (!result.errorMessage.isEmpty() ? QStringLiteral("failed")
               : (result.success ? QStringLiteral("completed") : QStringLiteral("idle")));
        return QJsonObject{
            {QStringLiteral("completedFrames"), progress.completedFrames.load()},
            {QStringLiteral("failedFrames"), progress.failedFrames.load()},
            {QStringLiteral("totalFrames"), progress.totalFrames},
            {QStringLiteral("remoteWorkers"), remoteWorkerCount()},
            {QStringLiteral("jobId"), impl_->currentJobId_},
            {QStringLiteral("compositionId"), impl_->currentCompositionId_.toString()},
            {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("status"), status},
            {QStringLiteral("busy"), busy},
            {QStringLiteral("queuedJobs"), queuedJobs},
            {QStringLiteral("paused"), paused},
            {QStringLiteral("templates"), QJsonArray::fromStringList(jobTemplateNames())},
            {QStringLiteral("jobHistory"), QJsonArray::fromStringList(jobHistory())},
            {QStringLiteral("success"), !busy && result.success},
            {QStringLiteral("errorMessage"), busy ? QString() : result.errorMessage},
            {QStringLiteral("elapsedMs"), progress.elapsedMs},
            {QStringLiteral("estimatedRemainingMs"), progress.estimatedRemainingMs},
            {QStringLiteral("estimatedCostMs"), estimatedCostMs},
            {QStringLiteral("workers"), remoteWorkerSnapshot()}
        };
    });
    const bool started = NetworkPCServer::instance().startHttpApi(port);
    if (!started) NetworkPCServer::instance().setHttpStatusProvider({});
    return started;
}

void RenderFarmMaster::stopHttpApi() {
    NetworkPCServer::instance().stopHttpApi();
    NetworkPCServer::instance().setHttpStatusProvider({});
}

bool RenderFarmMaster::isHttpApiRunning() const {
    return NetworkPCServer::instance().isHttpApiRunning();
}

unsigned short RenderFarmMaster::httpApiPort() const {
    return NetworkPCServer::instance().httpApiPort();
}

bool RenderFarmMaster::allowRemoteWorkers() const {
    return impl_->allowRemote_;
}

RenderFarmMaster& RenderFarmMaster::instance() {
    static RenderFarmMaster s_instance;
    return s_instance;
}

}
