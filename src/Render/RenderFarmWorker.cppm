module;
#include <utility>
#include <memory>
#include <atomic>
#include <mutex>
#include <QString>

module Render.Farm.Worker;

import Render.Farm.Types;
import Core.ThreadPool;

namespace ArtifactCore {

class RenderFarmWorker::Impl {
public:
    int id_;
    std::atomic<WorkerState> state_{ WorkerState::Idle };
    std::atomic<bool> cancelled_{ false };

    std::atomic<int> completed_{ 0 };
    std::atomic<int> failed_{ 0 };
    int totalFrames_ = 0;

    RenderJobResult result_;

    RenderFarmProgressCallback onProgress_;
    std::function<void(const RenderJobResult&)> onCompleted_;

    mutable std::mutex resultMutex_;

    explicit Impl(int id) : id_(id) {}

    void execute(const RenderJobRequest& request, const FrameRange& subRange) {
        state_.store(WorkerState::Rendering);
        cancelled_ = false;

        completed_.store(0);
        failed_.store(0);
        totalFrames_ = subRange.count();

        for (int frame = subRange.startFrame; frame < subRange.endFrame; frame += subRange.step) {
            if (cancelled_) break;

            try {
                if (request.renderFrame) {
                    request.renderFrame(frame);
                }
                completed_.fetch_add(1);
            }
            catch (...) {
                failed_.fetch_add(1);
            }
            if (onProgress_) {
                RenderJobProgress p;
                p.completedFrames.store(completed_.load());
                p.failedFrames.store(failed_.load());
                p.totalFrames = totalFrames_;
                onProgress_(p);
            }
        }

        state_.store(cancelled_ ? WorkerState::Cancelled : WorkerState::Completed);

        RenderJobResult result;
        int c = completed_.load();
        int f = failed_.load();
        result.success = f == 0;
        result.renderedFrames = c;
        result.failedFrames = f;
        if (f > 0) {
            result.errorMessage = QString("Worker %1: %2 frames failed").arg(id_).arg(f);
        }
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            result_ = result;
        }
        if (onCompleted_) {
            onCompleted_(result);
        }
    }
};

RenderFarmWorker::RenderFarmWorker(int id)
    : impl_(std::make_unique<Impl>(id))
{}

RenderFarmWorker::~RenderFarmWorker() = default;

int RenderFarmWorker::id() const {
    return impl_->id_;
}

void RenderFarmWorker::start(const RenderJobRequest& request, const FrameRange& subRange) {
    if (impl_->state_ == WorkerState::Rendering) return;
    impl_->execute(request, subRange);
}

void RenderFarmWorker::cancel() {
    impl_->cancelled_ = true;
}

bool RenderFarmWorker::isBusy() const {
    return impl_->state_ == WorkerState::Rendering;
}

WorkerState RenderFarmWorker::state() const {
    return impl_->state_;
}

RenderJobProgress RenderFarmWorker::progress() const {
    RenderJobProgress p;
    p.completedFrames.store(impl_->completed_.load());
    p.failedFrames.store(impl_->failed_.load());
    p.totalFrames = impl_->totalFrames_;
    return p;
}

void RenderFarmWorker::setOnProgress(RenderFarmProgressCallback callback) {
    impl_->onProgress_ = std::move(callback);
}

void RenderFarmWorker::setOnCompleted(std::function<void(const RenderJobResult&)> callback) {
    impl_->onCompleted_ = std::move(callback);
}

}
