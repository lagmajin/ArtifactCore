module;
#include <utility>
#include <memory>
#include <functional>
#include <atomic>
#include "../Define/DllExportMacro.hpp"

export module Render.Farm.Worker;

import Render.Farm.Types;
import Core.ThreadPool;

export namespace ArtifactCore {

class LIBRARY_DLL_API RenderFarmWorker {
public:
    explicit RenderFarmWorker(int id);
    ~RenderFarmWorker();

    RenderFarmWorker(const RenderFarmWorker&) = delete;
    RenderFarmWorker& operator=(const RenderFarmWorker&) = delete;

    int id() const;

    void start(const RenderJobRequest& request, const RenderFrameRange& subRange);
    void cancel();
    bool isBusy() const;

    WorkerState state() const;
    RenderJobProgress progress() const;

    void setOnProgress(RenderFarmProgressCallback callback);
    void setOnCompleted(std::function<void(const RenderJobResult&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
