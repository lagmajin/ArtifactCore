module;
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "../Define/DllExportMacro.hpp"

export module Graphics.GPUThreadPool;

export namespace ArtifactCore {

enum class GPUJobPriority : std::uint8_t { Low, Normal, High };

struct GPUThreadPoolSnapshot {
    std::size_t queued = 0;
    std::size_t submitted = 0;
    std::size_t completed = 0;
    std::size_t failed = 0;
    std::size_t cancelled = 0;
};

class LIBRARY_DLL_API GPUJobHandle {
public:
    struct State;
    GPUJobHandle() = default;
    bool isValid() const noexcept;
    bool isSubmitted() const noexcept;
    bool isCompleted() const noexcept;
    bool isFailed() const noexcept;
    bool isCancelled() const noexcept;
    std::uint64_t id() const noexcept;

private:
    std::shared_ptr<State> state_;
    explicit GPUJobHandle(std::shared_ptr<State> state);
    friend class GPUThreadPool;
};

class LIBRARY_DLL_API GPUThreadPool {
public:
    using GPUJob = std::function<void()>;
    using Executor = std::function<bool(const GPUJob&)>;

    explicit GPUThreadPool(std::size_t maxQueuedJobs = 4096);
    ~GPUThreadPool();
    GPUThreadPool(const GPUThreadPool&) = delete;
    GPUThreadPool& operator=(const GPUThreadPool&) = delete;

    GPUJobHandle enqueue(GPUJob job, GPUJobPriority priority = GPUJobPriority::Normal,
                         std::string label = {});
    std::size_t drain(const Executor& executor, std::size_t budget = 0);
    void cancelPending();
    void clearCompleted();
    GPUThreadPoolSnapshot snapshot() const;
    std::size_t maxQueuedJobs() const noexcept;

private:
    struct Impl;
    Impl* impl_;
};

}
