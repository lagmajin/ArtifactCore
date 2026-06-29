module;
#include <utility>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
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
    void cancelAll();
    bool isBusy() const;

    RenderJobProgress overallProgress() const;
    RenderJobResult result() const;

    void setRetryPolicy(const RetryPolicy& policy);
    void setCheckpointPolicy(const CheckpointPolicy& policy);
    CheckpointStore* checkpointStore() const;

    void setOnProgress(RenderFarmProgressCallback callback);
    void setOnCompleted(std::function<void(const RenderJobResult&)> callback);

    // -- Out-of-process worker support (Phase 4) --
    bool startRpcServer(unsigned short port = 0);
    void stopRpcServer();
    bool isRpcServerRunning() const;
    unsigned short rpcServerPort() const;

    int remoteWorkerCount() const;
    QStringList remoteWorkerIds() const;

    void setAllowRemoteWorkers(bool allow);
    bool allowRemoteWorkers() const;

    static RenderFarmMaster& instance();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
