module;
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include "../../include/Define/DllExportMacro.hpp"

module Graphics.GPUThreadPool;

namespace ArtifactCore {

struct GPUJobHandle::State {
    enum class Status : std::uint8_t { Queued, Submitted, Completed, Failed, Cancelled };
    std::uint64_t id = 0;
    std::atomic<Status> status{Status::Queued};
};

GPUJobHandle::GPUJobHandle(std::shared_ptr<State> state) : state_(std::move(state)) {}
bool GPUJobHandle::isValid() const noexcept { return static_cast<bool>(state_); }
bool GPUJobHandle::isSubmitted() const noexcept { return state_ && state_->status.load() != State::Status::Queued; }
bool GPUJobHandle::isCompleted() const noexcept { return state_ && state_->status.load() == State::Status::Completed; }
bool GPUJobHandle::isFailed() const noexcept { return state_ && state_->status.load() == State::Status::Failed; }
bool GPUJobHandle::isCancelled() const noexcept { return state_ && state_->status.load() == State::Status::Cancelled; }
std::uint64_t GPUJobHandle::id() const noexcept { return state_ ? state_->id : 0; }

struct GPUThreadPool::Impl {
    struct Entry { GPUJob job; GPUJobPriority priority; std::string label; std::shared_ptr<GPUJobHandle::State> state; };
    mutable std::mutex mutex;
    std::deque<Entry> pending;
    std::size_t maxQueuedJobs;
    std::uint64_t nextId = 1;
    GPUThreadPoolSnapshot stats;
};

GPUThreadPool::GPUThreadPool(std::size_t maxQueuedJobs) : impl_(new Impl{ {}, {}, std::max<std::size_t>(1, maxQueuedJobs) }) {}
GPUThreadPool::~GPUThreadPool() { cancelPending(); delete impl_; }

GPUJobHandle GPUThreadPool::enqueue(GPUJob job, GPUJobPriority priority, std::string label) {
    if (!job) return {};
    std::lock_guard lock(impl_->mutex);
    if (impl_->pending.size() >= impl_->maxQueuedJobs) return {};
    auto state = std::make_shared<GPUJobHandle::State>();
    state->id = impl_->nextId++;
    impl_->pending.push_back({std::move(job), priority, std::move(label), state});
    ++impl_->stats.queued;
    return GPUJobHandle(std::move(state));
}

std::size_t GPUThreadPool::drain(const Executor& executor, std::size_t budget) {
    if (!executor) return 0;
    std::size_t processed = 0;
    while (budget == 0 || processed < budget) {
        Impl::Entry entry;
        {
            std::lock_guard lock(impl_->mutex);
            if (impl_->pending.empty()) break;
            entry = std::move(impl_->pending.front());
            impl_->pending.pop_front();
            --impl_->stats.queued;
            ++impl_->stats.submitted;
        }
        entry.state->status.store(GPUJobHandle::State::Status::Submitted);
        const bool accepted = executor(entry.job);
        entry.state->status.store(accepted ? GPUJobHandle::State::Status::Completed : GPUJobHandle::State::Status::Failed);
        std::lock_guard lock(impl_->mutex);
        if (accepted) ++impl_->stats.completed; else ++impl_->stats.failed;
        ++processed;
    }
    return processed;
}

void GPUThreadPool::cancelPending() {
    std::lock_guard lock(impl_->mutex);
    for (auto& entry : impl_->pending) { entry.state->status.store(GPUJobHandle::State::Status::Cancelled); ++impl_->stats.cancelled; }
    impl_->pending.clear();
    impl_->stats.queued = 0;
}

void GPUThreadPool::clearCompleted() {
    std::lock_guard lock(impl_->mutex);
    impl_->stats = {};
    impl_->stats.queued = impl_->pending.size();
}

GPUThreadPoolSnapshot GPUThreadPool::snapshot() const { std::lock_guard lock(impl_->mutex); return impl_->stats; }
std::size_t GPUThreadPool::maxQueuedJobs() const noexcept { return impl_->maxQueuedJobs; }

}
