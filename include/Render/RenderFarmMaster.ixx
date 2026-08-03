module;
#include <utility>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <optional>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Master;

import Render.Farm.Types;
import Render.Farm.Checkpoint;
import Core.ThreadPool;

export namespace ArtifactCore {

class LIBRARY_DLL_API RenderFarmMaster {
public:
    explicit RenderFarmMaster(int workerCount = 0);
    ~RenderFarmMaster();

    RenderFarmMaster(const RenderFarmMaster&) = delete;
    RenderFarmMaster& operator=(const RenderFarmMaster&) = delete;

    void setWorkerCount(int count);
    int workerCount() const;

    void submitJob(const RenderJobRequest& request);
    bool cancelJob(const QString& jobId);
    int cancelJobs(const QStringList& jobIds);
    int clearQueuedJobs();
    bool setQueuedJobPriority(const QString& jobId, int priority);
    int setQueuedJobPriorities(const QStringList& jobIds, int priority);
    bool saveQueue(const QString& filePath) const;
    bool loadQueue(const QString& filePath);
    bool resubmitJob(const QString& jobId);
    int resubmitJobs(const QStringList& jobIds);
    bool duplicateJob(const QString& jobId, const QJsonObject& overrides = {});
    QStringList jobHistory() const;
    bool submitTemplate(const QString& name);
    bool registerJobTemplate(const RenderJobTemplate& jobTemplate);
    bool removeJobTemplate(const QString& name);
    QStringList jobTemplateNames() const;
    std::optional<RenderJobRequest> jobTemplate(const QString& name) const;
    bool saveJobTemplates(const QString& filePath) const;
    bool loadJobTemplates(const QString& filePath);
    void cancelAll();
    void pause();
    void resume();
    bool isPaused() const;
    bool isBusy() const;

    RenderJobProgress overallProgress() const;
    RenderJobResult result() const;

    void setRetryPolicy(const RetryPolicy& policy);
    void setCheckpointPolicy(const CheckpointPolicy& policy);
    CheckpointStore* checkpointStore() const;

    void setOnProgress(RenderFarmProgressCallback callback);
    void setOnCompleted(std::function<void(const RenderJobResult&)> callback);
    void setOnAlert(RenderFarmAlertCallback callback);
    void setFailureAlertThreshold(double fraction);
    void clearLastAlert();

    // -- Out-of-process worker support (Phase 4) --
    bool startRpcServer(unsigned short port = 0);
    void stopRpcServer();
    bool isRpcServerRunning() const;
    unsigned short rpcServerPort() const;

    int remoteWorkerCount() const;
    QStringList remoteWorkerIds() const;
    QJsonArray remoteWorkerSnapshot() const;

    void setAllowRemoteWorkers(bool allow);
    bool allowRemoteWorkers() const;
    void setRpcAuthToken(const QString& token);
    bool setRpcTlsCertificateFiles(const QString& certificateFile,
                                   const QString& privateKeyFile);
    bool setRemoteWorkerMaintenance(const QString& workerId, bool maintenance);
    bool startHttpApi(unsigned short port = 0);
    void stopHttpApi();
    bool isHttpApiRunning() const;
    unsigned short httpApiPort() const;

    static RenderFarmMaster& instance();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
