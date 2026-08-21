module;
#include <utility>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>
#include <limits>
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
#include <QSet>
#include <QRegularExpression>
#include <QSaveFile>
#include <QCoreApplication>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#ifdef emit
#undef emit
#endif

module Render.Farm.Master;

import Container.NamedVector;

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
    std::atomic<int> currentPriority_{ 0 };
    std::atomic<int> preemptionCount_{ 0 };
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
    mutable std::mutex callbackMutex_;
    double failureAlertThreshold_ = 0.0;
    int queuedJobAlertThreshold_ = 0;
    QString lastAlertType_;
    QDateTime lastAlertAt_;
    int lastAlertFailedFrames_ = 0;
    int lastAlertQueuedJobs_ = 0;
    QString alertWebhookUrl_;

    void postAlertWebhook(const QString& type, const RenderJobResult& result) {
        QString url;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            url = alertWebhookUrl_.trimmed();
        }
        QString jobId;
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            jobId = currentJobId_;
        }
        QCoreApplication* app = QCoreApplication::instance();
        if (url.isEmpty() || !app || !QUrl(url).isValid()) return;
        const QJsonObject payload{
            {QStringLiteral("text"), QStringLiteral("Artifact Render Farm: %1 (%2 frames, %3 ms)%4")
                .arg(type).arg(result.renderedFrames).arg(result.elapsedMs)
                .arg(result.errorMessage.isEmpty() ? QString() : QStringLiteral(" - ") + result.errorMessage)},
            {QStringLiteral("type"), type},
            {QStringLiteral("jobId"), jobId},
            {QStringLiteral("success"), result.success},
            {QStringLiteral("renderedFrames"), result.renderedFrames},
            {QStringLiteral("failedFrames"), result.failedFrames},
            {QStringLiteral("elapsedMs"), result.elapsedMs},
            {QStringLiteral("errorMessage"), result.errorMessage}
        };
        QMetaObject::invokeMethod(app, [url, payload]() {
            static QNetworkAccessManager manager;
            QNetworkRequest request{QUrl(url)};
            request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            QNetworkReply* reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
            QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
        }, Qt::QueuedConnection);
    }

    NamedVector<int> frameAttempts_;
    std::mutex frameAttemptsMutex_;

    std::thread farmThread_;
    std::deque<RenderJobRequest> pendingJobs_;
    mutable std::mutex pendingJobsMutex_;
    mutable QString queuePersistenceFile_;
    mutable std::mutex resultMutex_;
    std::chrono::steady_clock::time_point jobDeadline_{};
    std::chrono::steady_clock::time_point jobStartedAt_{};
    std::atomic<qint64> lastElapsedMs_{ 0 };
    bool hasJobDeadline_ = false;
    mutable std::mutex jobStateMutex_;

    QString currentJobId_;
    ArtifactCore::Id currentCompositionId_;

    // Phase 4: Remote worker support
    std::atomic<bool> allowRemote_{ false };
    std::atomic<unsigned short> rpcPort_{ 0 };
    std::atomic<bool> rpcRunning_{ false };
    NamedVector<RemoteJobSlice> remoteSlices_;
    std::mutex remoteMutex_;
    std::atomic<int> remoteCompleted_{0};
    std::atomic<int> totalRemoteFrames_{ 0 };
    std::mutex remoteWaitMutex_;
    std::condition_variable remoteCv_;
    std::function<void(const QString&, int, bool)> onRemoteFrameResult_;
    std::map<QString, RenderJobRequest> jobTemplates_;
    mutable std::mutex jobTemplatesMutex_;
    std::map<QString, RenderJobRequest> jobHistory_;
    std::map<QString, RenderJobResult> jobHistoryResults_;
    std::deque<QString> jobHistoryOrder_;
    mutable std::mutex jobHistoryMutex_;

    void emitProgress() {
        RenderFarmProgressCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = onProgress_;
        }
        if (!callback) return;
        int totalFrames = 0;
        std::chrono::steady_clock::time_point jobStartedAt;
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            totalFrames = totalFrames_;
            jobStartedAt = jobStartedAt_;
        }
        RenderJobProgress progress;
        progress.completedFrames.store(totalProgress_.completed.load());
        progress.failedFrames.store(totalProgress_.failed.load());
        progress.totalFrames = totalFrames;
        progress.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - jobStartedAt).count();
        const int processed = progress.completedFrames.load() +
            progress.failedFrames.load();
        if (processed > 0 && totalFrames > processed) {
            progress.estimatedRemainingMs = static_cast<qint64>(
                (static_cast<double>(progress.elapsedMs) / processed) *
                (totalFrames - processed));
        } else if (totalFrames <= processed) {
            progress.estimatedRemainingMs = 0;
        }
        callback(progress);
    }

    RenderFarmAlertCallback alertCallback() const {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        return onAlert_;
    }

    void notifyAlert(const QString& type, const RenderJobResult& result) {
        RenderFarmAlertCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = onAlert_;
        }
        if (callback) callback(type, result);
    }

    bool webhookConfigured() const {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        return !alertWebhookUrl_.trimmed().isEmpty();
    }

    QString currentJobIdSnapshot() const {
        std::lock_guard<std::mutex> lock(jobStateMutex_);
        return currentJobId_;
    }

    ArtifactCore::Id currentCompositionIdSnapshot() const {
        std::lock_guard<std::mutex> lock(jobStateMutex_);
        return currentCompositionId_;
    }

    bool deadlineReached() const {
        std::lock_guard<std::mutex> lock(jobStateMutex_);
        return hasJobDeadline_ && std::chrono::steady_clock::now() >= jobDeadline_;
    }

    std::chrono::milliseconds remainingJobTime() const {
        std::lock_guard<std::mutex> lock(jobStateMutex_);
        if (!hasJobDeadline_) return std::chrono::minutes(10);
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::max(std::chrono::steady_clock::duration::zero(),
                     jobDeadline_ - std::chrono::steady_clock::now()));
    }

    void notifyCompleted(const RenderJobResult& result) {
        RenderJobResult trackedResult = result;
        QString jobId;
        std::chrono::steady_clock::time_point jobStartedAt;
        int totalFrames = 0;
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            jobId = currentJobId_;
            jobStartedAt = jobStartedAt_;
            totalFrames = totalFrames_;
        }
        trackedResult.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - jobStartedAt).count();
        {
            std::lock_guard<std::mutex> lock(jobHistoryMutex_);
            if (!jobId.isEmpty()) jobHistoryResults_[jobId] = trackedResult;
        }
        std::function<void(const RenderJobResult&)> completedCallback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            completedCallback = onCompleted_;
        }
        if (completedCallback) completedCallback(trackedResult);
        postAlertWebhook(trackedResult.success ? QStringLiteral("job_completed")
                                                : QStringLiteral("job_failed"), trackedResult);
        double failureThreshold = 0.0;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            failureThreshold = failureAlertThreshold_;
        }
        if (alertCallback() && failureThreshold > 0.0 && totalFrames > 0) {
            const double failureFraction = static_cast<double>(trackedResult.failedFrames)
                / static_cast<double>(totalFrames);
            if (failureFraction >= failureThreshold)
            {
                {
                    std::lock_guard<std::mutex> lock(callbackMutex_);
                    lastAlertType_ = QStringLiteral("failure_rate");
                    lastAlertAt_ = QDateTime::currentDateTimeUtc();
                    lastAlertFailedFrames_ = trackedResult.failedFrames;
                }
                notifyAlert(QStringLiteral("failure_rate"), trackedResult);
                postAlertWebhook(QStringLiteral("failure_rate"), trackedResult);
            }
        }
    }

    static QJsonObject requestToJson(const RenderJobRequest& request) {
        return QJsonObject{
            {QStringLiteral("jobId"), request.jobId},
            {QStringLiteral("compositionId"), request.compositionId.toString()},
            {QStringLiteral("compositionName"), request.compositionName},
            {QStringLiteral("startFrame"), request.range.startFrame},
            {QStringLiteral("endFrame"), request.range.endFrame},
            {QStringLiteral("step"), request.range.step},
            {QStringLiteral("outputPath"), request.outputPath},
            {QStringLiteral("autoVersionOutput"), request.autoVersionOutput},
            {QStringLiteral("enableAudio"), request.enableAudio},
            {QStringLiteral("priority"), request.priority},
            {QStringLiteral("dependencies"), QJsonArray::fromStringList(request.dependencies)},
            {QStringLiteral("jobPool"), request.jobPool},
            {QStringLiteral("allowedWorkerIds"), QJsonArray::fromStringList(request.allowedWorkerIds)},
            {QStringLiteral("jobTimeoutMs"), request.jobTimeoutMs},
            {QStringLiteral("frameTimeoutMs"), request.frameTimeoutMs},
            {QStringLiteral("requiredCapabilities"), request.requiredCapabilities},
            {QStringLiteral("renderPayload"), request.renderPayload},
            {QStringLiteral("rendererExecutable"), request.rendererExecutable}
        };
    }

    static std::optional<RenderJobRequest> requestFromJson(const QJsonObject& object) {
        RenderJobRequest request;
        request.jobId = object.value(QStringLiteral("jobId")).toString().trimmed();
        request.compositionName = object.value(QStringLiteral("compositionName")).toString();
        const QString compositionId = object.value(QStringLiteral("compositionId")).toString();
        if (!compositionId.isEmpty()) request.compositionId = ArtifactCore::Id(compositionId);
        request.range.startFrame = object.value(QStringLiteral("startFrame")).toInt();
        request.range.endFrame = object.value(QStringLiteral("endFrame")).toInt();
        request.range.step = std::max(1, object.value(QStringLiteral("step")).toInt(1));
        request.outputPath = object.value(QStringLiteral("outputPath")).toString();
        request.autoVersionOutput = object.value(QStringLiteral("autoVersionOutput")).toBool(false);
        request.enableAudio = object.value(QStringLiteral("enableAudio")).toBool(false);
        request.priority = object.value(QStringLiteral("priority")).toInt();
        for (const auto& dependency : object.value(QStringLiteral("dependencies")).toArray())
            request.dependencies.push_back(dependency.toString().trimmed());
        request.jobPool = object.value(QStringLiteral("jobPool")).toString().trimmed();
        for (const auto& workerId : object.value(QStringLiteral("allowedWorkerIds")).toArray())
            request.allowedWorkerIds.push_back(workerId.toString().trimmed());
        request.jobTimeoutMs = std::max(0, object.value(QStringLiteral("jobTimeoutMs")).toInt());
        request.frameTimeoutMs = std::max(0, object.value(QStringLiteral("frameTimeoutMs")).toInt());
        request.requiredCapabilities = object.value(QStringLiteral("requiredCapabilities")).toObject();
        request.renderPayload = object.value(QStringLiteral("renderPayload")).toObject();
        request.rendererExecutable = object.value(QStringLiteral("rendererExecutable")).toString();
        if (request.range.count() <= 0 || request.renderPayload.isEmpty()
            || request.rendererExecutable.isEmpty()) return std::nullopt;
        return request;
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

    NamedVector<RenderFrameRange> splitRange(const RenderFrameRange& range, int parts) const {
        NamedVector<RenderFrameRange> subRanges;
        if (parts <= 0 || range.count() <= 0) return subRanges;

        int totalFrames = range.count();
        int baseChunk = std::max(1, totalFrames / parts);
        int remainder = totalFrames - baseChunk * parts;
        int current = range.startFrame;

        for (int i = 0; i < parts && current < range.endFrame; ++i) {
            int chunkSize = baseChunk + (i < remainder ? 1 : 0);
            const long long candidateEnd = static_cast<long long>(current) +
                                           static_cast<long long>(chunkSize) * range.step;
            int chunkEnd = static_cast<int>(std::min<long long>(candidateEnd, range.endFrame));
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

    bool dependenciesSatisfied(const RenderJobRequest& request) const {
        if (request.dependencies.isEmpty()) return true;
        std::lock_guard<std::mutex> lock(jobHistoryMutex_);
        for (const QString& dependency : request.dependencies) {
            const auto resultIt = jobHistoryResults_.find(dependency);
            if (resultIt == jobHistoryResults_.end() || !resultIt->second.success)
                return false;
        }
        return true;
    }

    bool dependencyFailed(const RenderJobRequest& request) const {
        std::lock_guard<std::mutex> lock(jobHistoryMutex_);
        for (const QString& dependency : request.dependencies) {
            const auto resultIt = jobHistoryResults_.find(dependency);
            if (resultIt != jobHistoryResults_.end() && !resultIt->second.success)
                return true;
        }
        return false;
    }

    bool createsDependencyCycle(const RenderJobRequest& request) const {
        std::lock_guard<std::mutex> lock(jobHistoryMutex_);
        QSet<QString> visiting;
        std::function<bool(const QString&)> visit = [&](const QString& jobId) {
            if (jobId == request.jobId) return true;
            if (visiting.contains(jobId)) return false;
            visiting.insert(jobId);
            const auto it = jobHistory_.find(jobId);
            if (it != jobHistory_.end()) {
                for (const QString& dependency : it->second.dependencies) {
                    if (visit(dependency)) return true;
                }
            }
            visiting.remove(jobId);
            return false;
        };
        for (const QString& dependency : request.dependencies) {
            if (visit(dependency)) return true;
        }
        return false;
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

    QString frameOutputPath(const QString& outputPath, int frame) const {
        QString framePath = outputPath.trimmed();
        if (framePath.contains(QStringLiteral("####"))) {
            framePath.replace(QStringLiteral("####"),
                              QStringLiteral("%1").arg(frame, 4, 10, QChar('0')));
            return framePath;
        }
        const int tokenStart = framePath.indexOf(QStringLiteral("%0"));
        if (tokenStart < 0) return {};
        const int d = framePath.indexOf(QChar('d'), tokenStart + 2);
        if (d <= tokenStart + 2) return {};
        bool ok = false;
        const int width = framePath.mid(tokenStart + 2, d - tokenStart - 2).toInt(&ok);
        if (!ok || width <= 0) return {};
        const QString token = framePath.mid(tokenStart, d - tokenStart + 1);
        framePath.replace(token, QStringLiteral("%1").arg(frame, width, 10, QChar('0')));
        return framePath;
    }

    QJsonObject outputManifest(const QString& outputPath,
                               const RenderFrameRange& range) const {
        QJsonArray frames;
        QJsonArray missingFrames;
        const bool sequence = outputPath.contains(QStringLiteral("####"))
            || outputPath.contains(QStringLiteral("%0"));
        if (sequence) {
            for (int frame = range.startFrame; frame < range.endFrame; frame += range.step) {
                const QString path = frameOutputPath(outputPath, frame);
                const QFileInfo info(path);
                if (!info.isFile() || info.size() <= 0) missingFrames.append(frame);
                frames.append(QJsonObject{{QStringLiteral("frame"), frame},
                                          {QStringLiteral("path"), path},
                                          {QStringLiteral("exists"), info.isFile()},
                                          {QStringLiteral("sizeBytes"), info.isFile() ? info.size() : 0}});
            }
        } else {
            const QFileInfo info(outputPath);
            if (!info.isFile() || info.size() <= 0) missingFrames.append(range.startFrame);
            frames.append(QJsonObject{{QStringLiteral("frame"), range.startFrame},
                                      {QStringLiteral("path"), outputPath},
                                      {QStringLiteral("exists"), info.isFile()},
                                      {QStringLiteral("sizeBytes"), info.isFile() ? info.size() : 0}});
        }
        return QJsonObject{{QStringLiteral("outputPath"), outputPath},
                           {QStringLiteral("frames"), frames},
                           {QStringLiteral("missingFrames"), missingFrames},
                           {QStringLiteral("complete"), missingFrames.isEmpty()}};
    }

    QString versionOutputPath(const QString& outputPath) const {
        const QString path = outputPath.trimmed();
        if (path.isEmpty()) return path;
        const QFileInfo info(path);
        const QString suffix = info.suffix();
        const QString base = suffix.isEmpty()
            ? path : path.left(path.size() - suffix.size() - 1);
        for (int version = 1; version <= 9999; ++version) {
            const QString candidate = QStringLiteral("%1_v%2%3")
                .arg(base).arg(version, 3, 10, QChar('0'))
                .arg(suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix);
            if (!candidate.contains(QStringLiteral("####"))
                && !candidate.contains(QRegularExpression(QStringLiteral("%0\\d+d")))) {
                if (!QFileInfo::exists(candidate)) return candidate;
                continue;
            }
            const QFileInfo candidateInfo(candidate);
            QString wildcard = candidateInfo.fileName()
                .replace(QStringLiteral("####"), QStringLiteral("*"));
            wildcard.replace(QRegularExpression(QStringLiteral("%0\\d+d")), QStringLiteral("*"));
            if (candidateInfo.dir().entryList({wildcard}, QDir::Files).isEmpty())
                return candidate;
        }
        return path;
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
        QString jobId;
        int totalFrames = 0;
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            jobId = currentJobId_;
            totalFrames = totalFrames_;
        }
        if (jobId.isEmpty()) return;

        int completed = totalProgress_.completed.load();
        if (completed <= 0) return;

        CheckpointInfo cp;
        cp.jobId = jobId;
        const long long completedUpTo = static_cast<long long>(baseFrame) + completed;
        cp.completedUpToFrame = static_cast<int>(std::clamp<long long>(
            completedUpTo, std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max()));  // absolute frame (exclusive)
        cp.totalFrames = totalFrames;
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            cp.failures = finalResult_.failures;
        }
        cp.updatedAt = QDateTime::currentDateTime();
        if (cp.createdAt.isNull()) cp.createdAt = cp.updatedAt;
        checkpointStore_->save(cp);
    }

    // -- Local rendering --
    void executeLocalRange(const RenderJobRequest& request, const RenderFrameRange& subRange,
                           std::atomic<int>& checkpointCounter) {
        for (int frame = subRange.startFrame; frame < subRange.endFrame;
             frame = frame > std::numeric_limits<int>::max() - subRange.step
                 ? subRange.endFrame : frame + subRange.step) {
            {
                std::unique_lock<std::mutex> lock(pauseMutex_);
                pauseCv_.wait(lock, [this]() { return !paused_.load() || cancelled_.load(); });
            }
            if (cancelled_ || deadlineReached()) {
                if (deadlineReached())
                    timedOut_ = true;
                cancelled_ = true;
                break;
            }

            const QString existingFramePath = frameOutputPath(request.outputPath, frame);
            if (!existingFramePath.isEmpty()) {
                const QFileInfo existingFrame(existingFramePath);
                if (existingFrame.isFile() && existingFrame.size() > 0) {
                    totalProgress_.completed.fetch_add(1);
                    if (checkpointPolicy_.mode == CheckpointPolicy::Mode::EveryNFrames) {
                        int c = ++checkpointCounter;
                        if (c >= checkpointPolicy_.interval) {
                            saveCheckpoint(frame);
                            checkpointCounter = 0;
                        }
                    }
                    continue;
                }
            }

            int attempt = 0;

            do {
                {
                    std::unique_lock<std::mutex> lock(pauseMutex_);
                    pauseCv_.wait(lock, [this]() { return !paused_.load() || cancelled_.load(); });
                }
                if (cancelled_ || deadlineReached()) {
                    if (deadlineReached())
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
                    {
                        std::lock_guard<std::mutex> lock(resultMutex_);
                        finalResult_.failures.setHeld(frame, true);
                    }
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
        NamedVector<RemoteWorkerInfo> activeWorkers;
        for (const auto& w : workers) {
            if (!w.workerId.isEmpty() && w.connected && w.assignedFrames == 0
                && w.state == QStringLiteral("Idle")
                && (request.allowedWorkerIds.isEmpty() || request.allowedWorkerIds.contains(w.workerId))
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
                    jobJson["autoVersionOutput"] = request.autoVersionOutput;
                    jobJson["enableAudio"] = request.enableAudio;
                    jobJson["priority"] = request.priority;
                    jobJson["jobPool"] = request.jobPool;
                    jobJson["allowedWorkerIds"] = QJsonArray::fromStringList(request.allowedWorkerIds);
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
        currentPriority_.store(request.priority);
        cancelled_ = false;
        paused_ = false;
        timedOut_ = false;
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            jobStartedAt_ = std::chrono::steady_clock::now();
        }

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

        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            hasJobDeadline_ = request.jobTimeoutMs > 0;
            if (hasJobDeadline_) {
                jobDeadline_ = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(request.jobTimeoutMs);
            }
        }

        // Clear remote state from previous job
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            remoteSlices_.clear();
        }
        remoteCompleted_ = 0;
        totalRemoteFrames_ = 0;

        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            currentJobId_ = request.jobId.isEmpty()
                ? QString::number(QDateTime::currentMSecsSinceEpoch())
                : request.jobId;
            totalFrames_ = request.range.count();
        }
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            currentCompositionId_ = request.compositionId;
        }

        totalProgress_ = WorkerProgress{};
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
        QString jobId;
        {
            std::lock_guard<std::mutex> lock(jobStateMutex_);
            jobId = currentJobId_;
        }
        if (checkpointPolicy_.mode != CheckpointPolicy::Mode::Disabled && !jobId.isEmpty()) {
            auto existing = checkpointStore_->load(jobId);
            if (existing) {
                restoreUpTo = std::clamp(existing->completedUpToFrame,
                                         request.range.startFrame,
                                         request.range.endFrame);
                const long long restoredFrames =
                    std::max<long long>(0, static_cast<long long>(restoreUpTo) -
                                           request.range.startFrame);
                const int alreadyDone = static_cast<int>(std::min<long long>(
                    restoredFrames, total));
                totalProgress_.completed.store(alreadyDone);
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
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            if (remoteSlices_.empty()) return;
        }

        int totalRemote = 0;
        {
            std::lock_guard<std::mutex> lock(remoteMutex_);
            for (const auto& slice : remoteSlices_)
                totalRemote += slice.range.count();
        }
        totalRemoteFrames_ = totalRemote;
        remoteCompleted_ = 0;

        const auto waitDuration = remainingJobTime();
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
    if (tracked.autoVersionOutput)
        tracked.outputPath = impl_->versionOutputPath(tracked.outputPath);
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
            impl_->jobHistoryResults_.erase(oldest);
        }
    }
    const bool dependenciesReady = impl_->dependenciesSatisfied(tracked);
    if (impl_->busy_ || !dependenciesReady) {
        const bool shouldPreempt = impl_->busy_
            && tracked.priority > impl_->currentPriority_.load();
        QString persistencePath;
        int queuedCount = 0;
        {
            std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
            const auto insertAt = std::find_if(impl_->pendingJobs_.begin(), impl_->pendingJobs_.end(),
                [&tracked](const RenderJobRequest& queued) {
                    return queued.priority < tracked.priority;
                });
            impl_->pendingJobs_.insert(insertAt, tracked);
            queuedCount = static_cast<int>(impl_->pendingJobs_.size());
            persistencePath = impl_->queuePersistenceFile_;
        }
        if (!persistencePath.isEmpty()) saveQueue(persistencePath);
        if (shouldPreempt) {
            impl_->preemptionCount_.fetch_add(1);
            impl_->cancelled_ = true;
            impl_->pauseCv_.notify_all();
        }
        int queuedAlertThreshold = 0;
        {
            std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
            queuedAlertThreshold = impl_->queuedJobAlertThreshold_;
        }
        if (impl_->alertCallback() && queuedAlertThreshold > 0
            && queuedCount >= queuedAlertThreshold) {
            {
                std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
                impl_->lastAlertType_ = QStringLiteral("queue_depth");
                impl_->lastAlertAt_ = QDateTime::currentDateTimeUtc();
                impl_->lastAlertQueuedJobs_ = queuedCount;
            }
            RenderJobResult alertResult;
            alertResult.errorMessage = QStringLiteral("Queued jobs exceeded the configured threshold.");
            impl_->notifyAlert(QStringLiteral("queue_depth"), alertResult);
            impl_->postAlertWebhook(QStringLiteral("queue_depth"), alertResult);
        }
        return;
    }
    if (impl_->farmThread_.joinable()) {
        impl_->farmThread_.join();
    }
    impl_->farmThread_ = std::thread([this, tracked]() {
        impl_->executeJob(tracked);
        while (true) {
            RenderJobRequest next;
            QString persistencePath;
            bool rejectedDependency = false;
            {
                std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
                const auto blocked = std::find_if(impl_->pendingJobs_.begin(), impl_->pendingJobs_.end(),
                    [this](const RenderJobRequest& candidate) {
                        return impl_->dependencyFailed(candidate);
                    });
                if (blocked != impl_->pendingJobs_.end()) {
                    next = *blocked;
                    impl_->pendingJobs_.erase(blocked);
                    rejectedDependency = true;
                }
                if (rejectedDependency) {
                    persistencePath = impl_->queuePersistenceFile_;
                }
                if (rejectedDependency) {
                    std::lock_guard<std::mutex> historyLock(impl_->jobHistoryMutex_);
                    RenderJobResult rejected;
                    rejected.errorMessage = QStringLiteral("A dependency job failed.");
                    impl_->jobHistoryResults_[next.jobId] = rejected;
                }
                if (rejectedDependency) {
                    // Continue below after persisting the removal.
                } else {
                const auto ready = std::find_if(impl_->pendingJobs_.begin(), impl_->pendingJobs_.end(),
                    [this](const RenderJobRequest& candidate) {
                        return impl_->dependenciesSatisfied(candidate);
                    });
                if (ready == impl_->pendingJobs_.end()) break;
                next = *ready;
                impl_->pendingJobs_.erase(ready);
                persistencePath = impl_->queuePersistenceFile_;
                }
            }
            if (!persistencePath.isEmpty()) saveQueue(persistencePath);
            if (rejectedDependency) continue;
            impl_->executeJob(next);
        }
    });
}

bool RenderFarmMaster::saveQueue(const QString& filePath) const {
    const QString path = filePath.trimmed();
    if (path.isEmpty()) return false;
    impl_->queuePersistenceFile_ = path;
    QJsonArray jobs;
    {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        for (const auto& job : impl_->pendingJobs_) {
            if (!job.renderPayload.isEmpty() && !job.rendererExecutable.isEmpty())
                jobs.append(Impl::requestToJson(job));
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QJsonObject document{{QStringLiteral("schemaVersion"), 1},
                               {QStringLiteral("jobs"), jobs}};
    if (file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 0)
        return false;
    return file.commit();
}

bool RenderFarmMaster::loadQueue(const QString& filePath) {
    const QString path = filePath.trimmed();
    if (path.isEmpty()) return false;
    impl_->queuePersistenceFile_ = path;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError
        || (!document.isArray() && !document.isObject())) return false;
    const QJsonArray jobs = document.isArray()
        ? document.array() : document.object().value(QStringLiteral("jobs")).toArray();
    bool loaded = false;
    for (const auto& value : jobs) {
        if (!value.isObject()) continue;
        const auto request = Impl::requestFromJson(value.toObject());
        if (!request) continue;
        submitJob(*request);
        loaded = true;
    }
    return loaded;
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

int RenderFarmMaster::resubmitJobs(const QStringList& jobIds) {
    int resubmitted = 0;
    for (const QString& jobId : jobIds)
        if (resubmitJob(jobId)) ++resubmitted;
    return resubmitted;
}

bool RenderFarmMaster::duplicateJob(const QString& jobId, const QJsonObject& overrides) {
    RenderJobRequest request;
    const QString sourceId = jobId.trimmed();
    {
        std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
        const auto it = impl_->jobHistory_.find(sourceId);
        if (it == impl_->jobHistory_.end()) return false;
        request = it->second;
    }
    request.jobId = overrides.value(QStringLiteral("jobId")).toString().trimmed();
    if (request.jobId.isEmpty())
        request.jobId = QStringLiteral("%1-copy-%2").arg(sourceId).arg(QDateTime::currentMSecsSinceEpoch());
    if (overrides.contains(QStringLiteral("outputPath")))
        request.outputPath = overrides.value(QStringLiteral("outputPath")).toString();
    if (overrides.contains(QStringLiteral("priority")))
        request.priority = overrides.value(QStringLiteral("priority")).toInt(request.priority);
    if (overrides.contains(QStringLiteral("jobPool")))
        request.jobPool = overrides.value(QStringLiteral("jobPool")).toString().trimmed();
    if (overrides.contains(QStringLiteral("allowedWorkerIds"))) {
        request.allowedWorkerIds.clear();
        for (const auto& workerId : overrides.value(QStringLiteral("allowedWorkerIds")).toArray())
            request.allowedWorkerIds.push_back(workerId.toString().trimmed());
    }
    if (overrides.contains(QStringLiteral("autoVersionOutput")))
        request.autoVersionOutput = overrides.value(QStringLiteral("autoVersionOutput")).toBool();
    if (overrides.contains(QStringLiteral("startFrame")))
        request.range.startFrame = overrides.value(QStringLiteral("startFrame")).toInt(request.range.startFrame);
    if (overrides.contains(QStringLiteral("endFrame")))
        request.range.endFrame = overrides.value(QStringLiteral("endFrame")).toInt(request.range.endFrame);
    if (overrides.contains(QStringLiteral("step")))
        request.range.step = std::max(1, overrides.value(QStringLiteral("step")).toInt(request.range.step));
    if (overrides.contains(QStringLiteral("requiredCapabilities")))
        request.requiredCapabilities = overrides.value(QStringLiteral("requiredCapabilities")).toObject();
    if (request.range.count() <= 0) return false;
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

bool RenderFarmMaster::saveJobTemplates(const QString& filePath) const {
    const QString path = filePath.trimmed();
    if (path.isEmpty()) return false;
    QJsonArray templates;
    {
        std::lock_guard<std::mutex> lock(impl_->jobTemplatesMutex_);
        for (const auto& [name, request] : impl_->jobTemplates_) {
            if (request.renderPayload.isEmpty() || request.rendererExecutable.isEmpty()) continue;
            QJsonObject entry = Impl::requestToJson(request);
            entry[QStringLiteral("name")] = name;
            templates.append(entry);
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QJsonObject document{{QStringLiteral("schemaVersion"), 1},
                               {QStringLiteral("templates"), templates}};
    if (file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 0)
        return false;
    return file.commit();
}

bool RenderFarmMaster::loadJobTemplates(const QString& filePath) {
    const QString path = filePath.trimmed();
    if (path.isEmpty()) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QJsonArray templates = document.object().value(QStringLiteral("templates")).toArray();
    bool loaded = false;
    for (const auto& value : templates) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        const auto request = Impl::requestFromJson(object);
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty() || !request) continue;
        if (registerJobTemplate({name, *request})) loaded = true;
    }
    return loaded;
}

void RenderFarmMaster::cancelAll() {
    impl_->cancelled_ = true;
    QString persistencePath;
    {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        impl_->pendingJobs_.clear();
        persistencePath = impl_->queuePersistenceFile_;
    }
    if (!persistencePath.isEmpty()) saveQueue(persistencePath);
    impl_->pauseCv_.notify_all();
}

bool RenderFarmMaster::cancelJob(const QString& jobId) {
    const QString requestedId = jobId.trimmed();
    if (requestedId.isEmpty()) return false;
    QString currentJobId;
    {
        std::lock_guard<std::mutex> lock(impl_->jobStateMutex_);
        currentJobId = impl_->currentJobId_;
    }
    if (currentJobId == requestedId && impl_->busy_) {
        impl_->cancelled_ = true;
        impl_->pauseCv_.notify_all();
        return true;
    }
    QString persistencePath;
    {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        const auto it = std::find_if(impl_->pendingJobs_.begin(), impl_->pendingJobs_.end(),
            [&requestedId](const RenderJobRequest& queued) {
                return queued.jobId == requestedId;
            });
        if (it == impl_->pendingJobs_.end()) return false;
        impl_->pendingJobs_.erase(it);
        persistencePath = impl_->queuePersistenceFile_;
    }
    if (!persistencePath.isEmpty()) saveQueue(persistencePath);
    return true;
}

int RenderFarmMaster::cancelJobs(const QStringList& jobIds) {
    int cancelled = 0;
    for (const QString& jobId : jobIds)
        if (cancelJob(jobId)) ++cancelled;
    return cancelled;
}

int RenderFarmMaster::clearQueuedJobs() {
    int removed = 0;
    QString persistencePath;
    {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        removed = static_cast<int>(impl_->pendingJobs_.size());
        impl_->pendingJobs_.clear();
        persistencePath = impl_->queuePersistenceFile_;
    }
    if (!persistencePath.isEmpty()) saveQueue(persistencePath);
    return removed;
}

bool RenderFarmMaster::setQueuedJobPriority(const QString& jobId, int priority) {
    const QString requestedId = jobId.trimmed();
    QString persistencePath;
    {
        std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
        const auto it = std::find_if(impl_->pendingJobs_.begin(), impl_->pendingJobs_.end(),
            [&requestedId](const RenderJobRequest& queued) {
                return queued.jobId == requestedId;
            });
        if (it == impl_->pendingJobs_.end()) return false;
        RenderJobRequest updated = *it;
        updated.priority = priority;
        impl_->pendingJobs_.erase(it);
        const auto insertAt = std::find_if(impl_->pendingJobs_.begin(), impl_->pendingJobs_.end(),
            [&updated](const RenderJobRequest& queued) {
                return queued.priority < updated.priority;
            });
        impl_->pendingJobs_.insert(insertAt, std::move(updated));
        persistencePath = impl_->queuePersistenceFile_;
    }
    {
        std::lock_guard<std::mutex> historyLock(impl_->jobHistoryMutex_);
        const auto historyIt = impl_->jobHistory_.find(requestedId);
        if (historyIt != impl_->jobHistory_.end())
            historyIt->second.priority = priority;
    }
    if (!persistencePath.isEmpty()) saveQueue(persistencePath);
    return true;
}

int RenderFarmMaster::setQueuedJobPriorities(const QStringList& jobIds, int priority) {
    int updated = 0;
    for (const QString& jobId : jobIds)
        if (setQueuedJobPriority(jobId, priority)) ++updated;
    return updated;
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
    int totalFrames = 0;
    std::chrono::steady_clock::time_point jobStartedAt;
    {
        std::lock_guard<std::mutex> lock(impl_->jobStateMutex_);
        totalFrames = impl_->totalFrames_;
        jobStartedAt = impl_->jobStartedAt_;
    }
    p.totalFrames = totalFrames;
    if (impl_->busy_) {
        p.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - jobStartedAt).count();
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
    } else if (totalFrames <= processed) {
        p.estimatedRemainingMs = 0;
    }
    return p;
}

RenderJobResult RenderFarmMaster::result() const {
    std::lock_guard<std::mutex> lock(impl_->resultMutex_);
    return impl_->finalResult_;
}

void RenderFarmMaster::setOnProgress(RenderFarmProgressCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
    impl_->onProgress_ = std::move(callback);
}

void RenderFarmMaster::setOnCompleted(std::function<void(const RenderJobResult&)> callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
    impl_->onCompleted_ = std::move(callback);
}

void RenderFarmMaster::setOnAlert(RenderFarmAlertCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
    impl_->onAlert_ = std::move(callback);
}

void RenderFarmMaster::setFailureAlertThreshold(double fraction) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
    impl_->failureAlertThreshold_ = std::clamp(fraction, 0.0, 1.0);
}

void RenderFarmMaster::setQueuedJobAlertThreshold(int count) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
    impl_->queuedJobAlertThreshold_ = std::max(0, count);
}

void RenderFarmMaster::setAlertWebhookUrl(const QString& url) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
    impl_->alertWebhookUrl_ = url.trimmed();
}

void RenderFarmMaster::clearLastAlert() {
    {
        std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
        impl_->lastAlertType_.clear();
        impl_->lastAlertAt_ = QDateTime();
        impl_->lastAlertFailedFrames_ = 0;
        impl_->lastAlertQueuedJobs_ = 0;
    }
}

void RenderFarmMaster::setRetryPolicy(const RetryPolicy& policy) {
    impl_->retryPolicy_ = policy;
}

void RenderFarmMaster::setCheckpointPolicy(const CheckpointPolicy& policy) {
    CheckpointPolicy sanitized = policy;
    if (sanitized.mode != CheckpointPolicy::Mode::Disabled) {
        sanitized.interval = std::max(1, sanitized.interval);
    }
    impl_->checkpointPolicy_ = sanitized;
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
        int unreportedFrames = 0;
        {
            std::lock_guard<std::mutex> lock(impl_->remoteMutex_);
            for (auto& slice : impl_->remoteSlices_) {
                if (slice.workerId == workerId && !slice.completed) {
                    int remaining = slice.range.count() - slice.framesCompleted_;
                    if (remaining > 0)
                        impl_->remoteCompleted_.fetch_add(remaining);
                    unreportedFrames += std::max(0, remaining);
                }
            }
        }
        impl_->remoteCv_.notify_all();
        if (unreportedFrames > 0
            && (impl_->alertCallback() || impl_->webhookConfigured())) {
            RenderJobResult alertResult;
            alertResult.success = false;
            alertResult.failedFrames = unreportedFrames;
            alertResult.errorMessage = QStringLiteral("Remote worker disconnected: %1").arg(workerId);
            {
                std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
                impl_->lastAlertType_ = QStringLiteral("worker_disconnected");
                impl_->lastAlertAt_ = QDateTime::currentDateTimeUtc();
            }
            impl_->notifyAlert(QStringLiteral("worker_disconnected"), alertResult);
            impl_->postAlertWebhook(QStringLiteral("worker_disconnected"), alertResult);
        }
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
        if (method == QStringLiteral("validateOutput")) {
            RenderFrameRange range;
            range.startFrame = params.value(QStringLiteral("startFrame")).toInt(0);
            range.endFrame = params.value(QStringLiteral("endFrame")).toInt(0);
            range.step = std::max(1, params.value(QStringLiteral("step")).toInt(1));
            if (range.count() <= 0)
                return {{QStringLiteral("status"), QStringLiteral("invalid_range")}};
            const QString outputPath = params.value(QStringLiteral("outputPath")).toString();
            const QString pathError = impl_->validateOutputPath(outputPath);
            if (!pathError.isEmpty())
                return {{QStringLiteral("status"), QStringLiteral("invalid_output_path")},
                        {QStringLiteral("error"), pathError}};
            if (params.value(QStringLiteral("checkExisting")).toBool(false)) {
                const QString artifactError = impl_->validateOutputArtifact(outputPath, range);
                if (!artifactError.isEmpty())
                    return {{QStringLiteral("status"), QStringLiteral("invalid_output_artifact")},
                            {QStringLiteral("error"), artifactError}};
            }
            return {{QStringLiteral("status"), QStringLiteral("ok")},
                    {QStringLiteral("outputPath"), outputPath}};
        }
        if (method == QStringLiteral("verifyJobOutput")) {
            const QString jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            RenderJobRequest request;
            {
                std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
                const auto it = impl_->jobHistory_.find(jobId);
                if (it == impl_->jobHistory_.end())
                    return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
                request = it->second;
            }
            const QString error = impl_->validateOutputArtifact(request.outputPath, request.range);
            if (!error.isEmpty())
                return {{QStringLiteral("status"), QStringLiteral("invalid_output_artifact")},
                        {QStringLiteral("jobId"), jobId},
                        {QStringLiteral("error"), error}};
            return {{QStringLiteral("status"), QStringLiteral("ok")},
                    {QStringLiteral("jobId"), jobId},
                    {QStringLiteral("outputPath"), request.outputPath}};
        }
        if (method == QStringLiteral("inspectJobOutput")) {
            const QString jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
            const auto it = impl_->jobHistory_.find(jobId);
            if (it == impl_->jobHistory_.end())
                return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            QJsonObject manifest = impl_->outputManifest(it->second.outputPath, it->second.range);
            manifest[QStringLiteral("status")] = QStringLiteral("ok");
            manifest[QStringLiteral("jobId")] = jobId;
            return manifest;
        }
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
            request.autoVersionOutput = params.value(QStringLiteral("autoVersionOutput")).toBool(false);
            request.jobPool = params.value(QStringLiteral("jobPool")).toString().trimmed();
            for (const auto& workerId : params.value(QStringLiteral("allowedWorkerIds")).toArray())
                request.allowedWorkerIds.push_back(workerId.toString().trimmed());
            request.jobTimeoutMs = std::max(0, params.value(QStringLiteral("jobTimeoutMs")).toInt(0));
            request.frameTimeoutMs = std::max(0, params.value(QStringLiteral("frameTimeoutMs")).toInt(0));
            for (const auto& dependency : params.value(QStringLiteral("dependencies")).toArray())
                request.dependencies.push_back(dependency.toString().trimmed());
            request.outputPath = params.value(QStringLiteral("outputPath")).toString();
            request.rendererExecutable = params.value(QStringLiteral("rendererExecutable")).toString();
            request.renderPayload = params.value(QStringLiteral("renderPayload")).toObject();
            request.requiredCapabilities = params.value(QStringLiteral("requiredCapabilities")).toObject();
            const QString workerPool = params.value(QStringLiteral("workerPool")).toString().trimmed();
            if (!workerPool.isEmpty()) {
                request.requiredCapabilities[QStringLiteral("pool")] = workerPool;
            }
            const QJsonArray chunks = params.value(QStringLiteral("chunks")).toArray();
            if (request.dependencies.contains(request.jobId)
                || impl_->createsDependencyCycle(request)
                || (chunks.isEmpty() && request.range.endFrame <= request.range.startFrame)
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
            if (!chunks.isEmpty()) {
                QJsonArray jobIds;
                int index = 0;
                for (const auto& value : chunks) {
                    const QJsonObject chunk = value.toObject();
                    RenderJobRequest child = request;
                    child.range.startFrame = chunk.value(QStringLiteral("startFrame")).toInt(-1);
                    child.range.endFrame = chunk.value(QStringLiteral("endFrame")).toInt(-1);
                    child.range.step = std::max(1, chunk.value(QStringLiteral("step")).toInt(request.range.step));
                    if (child.range.count() <= 0)
                        return {{QStringLiteral("status"), QStringLiteral("invalid_chunk")}};
                    child.jobId = QStringLiteral("%1-chunk-%2").arg(request.jobId).arg(index++);
                    submitJob(child);
                    jobIds.append(child.jobId);
                }
                return {{QStringLiteral("status"), QStringLiteral("accepted")},
                        {QStringLiteral("jobId"), request.jobId},
                        {QStringLiteral("jobIds"), jobIds},
                        {QStringLiteral("queued"), queued}};
            }
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
        if (method == QStringLiteral("removeTemplate")) {
            const QString name = params.value(QStringLiteral("name")).toString().trimmed();
            if (!removeJobTemplate(name))
                return {{QStringLiteral("status"), QStringLiteral("template_not_found")}};
            return {{QStringLiteral("status"), QStringLiteral("removed")},
                    {QStringLiteral("name"), name}};
        }
        if (method == QStringLiteral("setFailureAlertThreshold")) {
            const double threshold = params.value(QStringLiteral("fraction")).toDouble(-1.0);
            if (threshold < 0.0 || threshold > 1.0)
                return {{QStringLiteral("status"), QStringLiteral("invalid_request")}};
            setFailureAlertThreshold(threshold);
            return {{QStringLiteral("status"), QStringLiteral("updated")},
                    {QStringLiteral("fraction"), threshold}};
        }
        if (method == QStringLiteral("setQueuedJobAlertThreshold")) {
            const int count = params.value(QStringLiteral("count")).toInt(-1);
            if (count < 0)
                return {{QStringLiteral("status"), QStringLiteral("invalid_request")}};
            setQueuedJobAlertThreshold(count);
            return {{QStringLiteral("status"), QStringLiteral("updated")},
                    {QStringLiteral("count"), count}};
        }
        if (method == QStringLiteral("setAlertWebhookUrl")) {
            const QString url = params.value(QStringLiteral("url")).toString().trimmed();
            const QUrl parsedUrl(url);
            if (!url.isEmpty() && (!parsedUrl.isValid()
                                   || (parsedUrl.scheme() != QStringLiteral("http")
                                       && parsedUrl.scheme() != QStringLiteral("https"))))
                return {{QStringLiteral("status"), QStringLiteral("invalid_request")}};
            setAlertWebhookUrl(url);
            return {{QStringLiteral("status"), QStringLiteral("updated")},
                    {QStringLiteral("configured"), !url.isEmpty()}};
        }
        if (method == QStringLiteral("clearLastAlert")) {
            clearLastAlert();
            return {{QStringLiteral("status"), QStringLiteral("cleared")}};
        }
        if (method == QStringLiteral("resubmitJob")) {
            const QString jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            if (!jobHistory().contains(jobId))
                return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            resubmitJob(jobId);
            return {{QStringLiteral("status"), QStringLiteral("accepted")}};
        }
        if (method == QStringLiteral("resubmitJobs")) {
            const QJsonArray ids = params.value(QStringLiteral("jobIds")).toArray();
            QStringList jobIds;
            for (const auto& value : ids) {
                const QString jobId = value.toString().trimmed();
                if (!jobId.isEmpty()) jobIds.push_back(jobId);
            }
            return {{QStringLiteral("status"), QStringLiteral("accepted")},
                    {QStringLiteral("resubmitted"), resubmitJobs(jobIds)}};
        }
        if (method == QStringLiteral("duplicateJob")) {
            const QString jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            if (!duplicateJob(jobId, params))
                return {{QStringLiteral("status"), QStringLiteral("job_not_found_or_invalid")}};
            return {{QStringLiteral("status"), QStringLiteral("accepted")}};
        }
        if (method == QStringLiteral("cancelJob")) {
            const QString requestedJobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            if (!requestedJobId.isEmpty()) {
                if (!cancelJob(requestedJobId))
                    return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
                return {{QStringLiteral("status"), QStringLiteral("cancel_requested")},
                        {QStringLiteral("jobId"), requestedJobId}};
            }
            cancelAll();
            return {{QStringLiteral("status"), QStringLiteral("cancel_requested")}};
        }
        if (method == QStringLiteral("cancelJobs")) {
            const QJsonArray ids = params.value(QStringLiteral("jobIds")).toArray();
            QStringList jobIds;
            for (const auto& value : ids) {
                const QString jobId = value.toString().trimmed();
                if (!jobId.isEmpty()) jobIds.push_back(jobId);
            }
            return {{QStringLiteral("status"), QStringLiteral("cancel_requested")},
                    {QStringLiteral("cancelled"), cancelJobs(jobIds)}};
        }
        if (method == QStringLiteral("clearQueuedJobs")) {
            return {{QStringLiteral("status"), QStringLiteral("cleared")},
                    {QStringLiteral("removed"), clearQueuedJobs()}};
        }
        if (method == QStringLiteral("setJobPriority")) {
            const QString jobId = params.value(QStringLiteral("jobId")).toString().trimmed();
            const int priority = params.value(QStringLiteral("priority")).toInt();
            if (!setQueuedJobPriority(jobId, priority))
                return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            return {{QStringLiteral("status"), QStringLiteral("updated")},
                    {QStringLiteral("jobId"), jobId},
                    {QStringLiteral("priority"), priority}};
        }
        if (method == QStringLiteral("setJobPriorities")) {
            const QJsonArray ids = params.value(QStringLiteral("jobIds")).toArray();
            QStringList jobIds;
            for (const auto& value : ids) {
                const QString jobId = value.toString().trimmed();
                if (!jobId.isEmpty()) jobIds.push_back(jobId);
            }
            const int priority = params.value(QStringLiteral("priority")).toInt();
            return {{QStringLiteral("status"), QStringLiteral("updated")},
                    {QStringLiteral("updated"), setQueuedJobPriorities(jobIds, priority)},
                    {QStringLiteral("priority"), priority}};
        }
        if (method == QStringLiteral("pauseJob")) {
            if (!isBusy()) return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            pause();
            return {{QStringLiteral("status"), QStringLiteral("pause_requested")},
                    {QStringLiteral("jobId"), impl_->currentJobIdSnapshot()}};
        }
        if (method == QStringLiteral("resumeJob")) {
            if (!isBusy()) return {{QStringLiteral("status"), QStringLiteral("job_not_found")}};
            resume();
            return {{QStringLiteral("status"), QStringLiteral("resume_requested")},
                    {QStringLiteral("jobId"), impl_->currentJobIdSnapshot()}};
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
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& worker : NetworkPCServer::instance().connectedWorkers()) {
        const qint64 heartbeatAgeMs = worker.lastHeartbeat > 0
            ? std::max<qint64>(0, now - worker.lastHeartbeat) : -1;
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
            {QStringLiteral("heartbeatLatencyMs"), worker.heartbeatLatencyMs},
            {QStringLiteral("heartbeatAgeMs"), heartbeatAgeMs},
            {QStringLiteral("healthy"), worker.connected && heartbeatAgeMs >= 0
                && heartbeatAgeMs <= 30000},
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
        QJsonArray queuedJobIds;
        QJsonArray queuedPriorities;
        int waitingDependencyJobs = 0;
        QJsonArray historyDetails;
        QJsonArray templateDetails;
        {
            std::lock_guard<std::mutex> lock(impl_->pendingJobsMutex_);
            queuedJobs = static_cast<int>(impl_->pendingJobs_.size());
            for (const auto& queued : impl_->pendingJobs_) {
                queuedJobIds.append(queued.jobId);
                queuedPriorities.append(queued.priority);
            }
        }
        {
            std::lock_guard<std::mutex> lock(impl_->jobHistoryMutex_);
            for (const auto& [jobId, request] : impl_->jobHistory_) {
                const auto resultIt = impl_->jobHistoryResults_.find(jobId);
                if (resultIt == impl_->jobHistoryResults_.end()) {
                    bool dependenciesReady = true;
                    for (const QString& dependency : request.dependencies) {
                        const auto dependencyResult = impl_->jobHistoryResults_.find(dependency);
                        if (dependencyResult == impl_->jobHistoryResults_.end()
                            || !dependencyResult->second.success) {
                            dependenciesReady = false;
                            break;
                        }
                    }
                    if (!dependenciesReady) ++waitingDependencyJobs;
                    historyDetails.append(QJsonObject{{QStringLiteral("jobId"), jobId},
                                                      {QStringLiteral("status"), dependenciesReady
                                                          ? QStringLiteral("queued")
                                                          : QStringLiteral("waiting_dependency")},
                                                      {QStringLiteral("priority"), request.priority},
                                                      {QStringLiteral("dependencies"), QJsonArray::fromStringList(request.dependencies)},
                                                      {QStringLiteral("jobPool"), request.jobPool},
                                                      {QStringLiteral("allowedWorkerIds"), QJsonArray::fromStringList(request.allowedWorkerIds)},
                                                      {QStringLiteral("compositionId"), request.compositionId.toString()},
                                                      {QStringLiteral("outputPath"), request.outputPath},
                                                      {QStringLiteral("autoVersionOutput"), request.autoVersionOutput}});
                    continue;
                }
                const auto& result = resultIt->second;
                historyDetails.append(QJsonObject{
                    {QStringLiteral("jobId"), jobId},
                    {QStringLiteral("priority"), request.priority},
                    {QStringLiteral("dependencies"), QJsonArray::fromStringList(request.dependencies)},
                    {QStringLiteral("jobPool"), request.jobPool},
                    {QStringLiteral("allowedWorkerIds"), QJsonArray::fromStringList(request.allowedWorkerIds)},
                    {QStringLiteral("compositionId"), request.compositionId.toString()},
                    {QStringLiteral("outputPath"), request.outputPath},
                    {QStringLiteral("autoVersionOutput"), request.autoVersionOutput},
                    {QStringLiteral("status"), result.success ? QStringLiteral("completed")
                                                                : QStringLiteral("failed")},
                    {QStringLiteral("success"), result.success},
                    {QStringLiteral("renderedFrames"), result.renderedFrames},
                    {QStringLiteral("failedFrames"), result.failedFrames},
                    {QStringLiteral("elapsedMs"), result.elapsedMs},
                    {QStringLiteral("errorMessage"), result.errorMessage}
                });
            }
        }
        {
            std::lock_guard<std::mutex> lock(impl_->jobTemplatesMutex_);
            for (const auto& [name, request] : impl_->jobTemplates_) {
                templateDetails.append(QJsonObject{
                    {QStringLiteral("name"), name},
                    {QStringLiteral("compositionId"), request.compositionId.toString()},
                    {QStringLiteral("startFrame"), request.range.startFrame},
                    {QStringLiteral("endFrame"), request.range.endFrame},
                    {QStringLiteral("step"), request.range.step},
                    {QStringLiteral("priority"), request.priority},
                    {QStringLiteral("outputPath"), request.outputPath},
                    {QStringLiteral("jobPool"), request.jobPool},
                    {QStringLiteral("autoVersionOutput"), request.autoVersionOutput},
                    {QStringLiteral("allowedWorkerIds"), QJsonArray::fromStringList(request.allowedWorkerIds)},
                    {QStringLiteral("requiredCapabilities"), request.requiredCapabilities}
                });
            }
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
        double failureAlertThreshold = 0.0;
        int queuedJobAlertThreshold = 0;
        bool alertWebhookConfigured = false;
        QString lastAlertType;
        QDateTime lastAlertAt;
        int lastAlertFailedFrames = 0;
        int lastAlertQueuedJobs = 0;
        {
            std::lock_guard<std::mutex> lock(impl_->callbackMutex_);
            failureAlertThreshold = impl_->failureAlertThreshold_;
            queuedJobAlertThreshold = impl_->queuedJobAlertThreshold_;
            alertWebhookConfigured = !impl_->alertWebhookUrl_.trimmed().isEmpty();
            lastAlertType = impl_->lastAlertType_;
            lastAlertAt = impl_->lastAlertAt_;
            lastAlertFailedFrames = impl_->lastAlertFailedFrames_;
            lastAlertQueuedJobs = impl_->lastAlertQueuedJobs_;
        }
        return QJsonObject{
            {QStringLiteral("completedFrames"), progress.completedFrames.load()},
            {QStringLiteral("failedFrames"), progress.failedFrames.load()},
            {QStringLiteral("totalFrames"), progress.totalFrames},
            {QStringLiteral("remoteWorkers"), remoteWorkerCount()},
            {QStringLiteral("jobId"), impl_->currentJobIdSnapshot()},
            {QStringLiteral("compositionId"), impl_->currentCompositionIdSnapshot().toString()},
            {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("status"), status},
            {QStringLiteral("busy"), busy},
            {QStringLiteral("preemptionCount"), impl_->preemptionCount_.load()},
            {QStringLiteral("queuedJobs"), queuedJobs},
            {QStringLiteral("queuedJobIds"), queuedJobIds},
            {QStringLiteral("queuedPriorities"), queuedPriorities},
            {QStringLiteral("waitingDependencyJobs"), waitingDependencyJobs},
            {QStringLiteral("failureAlertThreshold"), failureAlertThreshold},
            {QStringLiteral("queuedJobAlertThreshold"), queuedJobAlertThreshold},
            {QStringLiteral("alertWebhookConfigured"), alertWebhookConfigured},
            {QStringLiteral("lastAlertType"), lastAlertType},
            {QStringLiteral("lastAlertAt"), lastAlertAt.toString(Qt::ISODateWithMs)},
            {QStringLiteral("lastAlertFailedFrames"), lastAlertFailedFrames},
            {QStringLiteral("lastAlertQueuedJobs"), lastAlertQueuedJobs},
            {QStringLiteral("paused"), paused},
            {QStringLiteral("templates"), QJsonArray::fromStringList(jobTemplateNames())},
            {QStringLiteral("templateDetails"), templateDetails},
            {QStringLiteral("jobHistory"), QJsonArray::fromStringList(jobHistory())},
            {QStringLiteral("jobHistoryDetails"), historyDetails},
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
