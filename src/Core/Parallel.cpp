module;
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/task_arena.h>

module Core.Parallel;

namespace ArtifactCore {

namespace {

struct SharedParallelArenaState {
    std::mutex initializeMutex;
    std::unique_ptr<tbb::task_arena> owner;
    std::atomic<tbb::task_arena*> published{nullptr};
};

SharedParallelArenaState& sharedParallelArenaState()
{
    static SharedParallelArenaState state;
    return state;
}

tbb::task_arena& ensureSharedParallelArena(int requestedConcurrency)
{
    auto& state = sharedParallelArenaState();
    if (auto* arena = state.published.load(std::memory_order_acquire)) {
        return *arena;
    }

    std::lock_guard<std::mutex> lock(state.initializeMutex);
    if (!state.owner) {
        const int concurrency = requestedConcurrency > 0
            ? requestedConcurrency : tbb::task_arena::automatic;
        state.owner = std::make_unique<tbb::task_arena>(concurrency);
        state.owner->initialize();
        state.published.store(state.owner.get(), std::memory_order_release);
    }
    return *state.owner;
}

} // namespace

int Parallel::InitializeSharedArena(int maxConcurrency)
{
    return ensureSharedParallelArena(maxConcurrency).max_concurrency();
}

int Parallel::SharedArenaConcurrency()
{
    return ensureSharedParallelArena(0).max_concurrency();
}

void Parallel::ExecuteInSharedArena(const std::function<void()>& func)
{
    if (!func) {
        return;
    }
    auto* context = const_cast<std::function<void()>*>(&func);
    ExecuteInSharedArenaErased(context, [](void* opaque) {
        (*static_cast<const std::function<void()>*>(opaque))();
    });
}

void Parallel::ExecuteInSharedArenaErased(void* context,
                                         Parallel::ArenaInvoker invoke)
{
    if (!invoke) {
        return;
    }
    ensureSharedParallelArena(0).execute([context, invoke]() {
        invoke(context);
    });
}

void Parallel::ForErased(int start, int end, void* context,
                         Parallel::WorkInvoker invoke) {
    if (start >= end || !invoke) return;
    ensureSharedParallelArena(0).execute([&]() {
        tbb::parallel_for(start, end, [context, invoke](int index) {
            invoke(context, index);
        });
    });
}

void Parallel::ForSizeErased(std::size_t start, std::size_t end,
                             void* context,
                             Parallel::SizeWorkInvoker invoke) {
    if (start >= end || !invoke) return;
    ensureSharedParallelArena(0).execute([&]() {
        tbb::parallel_for(start, end, [context, invoke](std::size_t index) {
            invoke(context, index);
        });
    });
}

void Parallel::ForSizeErasedWithGrain(std::size_t start, std::size_t end,
                                      std::size_t grainSize,
                                      void* context,
                                      Parallel::SizeWorkInvoker invoke) {
    if (start >= end || !invoke) return;
    const std::size_t effectiveGrainSize = std::max<std::size_t>(1, grainSize);
    ensureSharedParallelArena(0).execute([&]() {
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(start, end, effectiveGrainSize),
            [context, invoke](const tbb::blocked_range<std::size_t>& range) {
                for (std::size_t index = range.begin(); index != range.end(); ++index) {
                    invoke(context, index);
                }
            });
    });
}

void Parallel::ForErased(int start, int end,
                         const std::function<void(int)>& func) {
    if (start >= end || !func) return;
    ensureSharedParallelArena(0).execute([&]() {
        tbb::parallel_for(start, end, func);
    });
}

void Parallel::ForErasedWithGrain(int start, int end, int grainSize,
                                  void* context,
                                  Parallel::WorkInvoker invoke) {
    if (start >= end || !invoke) return;
    const int effectiveGrainSize = std::max(1, grainSize);
    ensureSharedParallelArena(0).execute([&]() {
        tbb::parallel_for(
            tbb::blocked_range<int>(start, end, effectiveGrainSize),
            [context, invoke](const tbb::blocked_range<int>& range) {
                for (int index = range.begin(); index != range.end(); ++index) {
                    invoke(context, index);
                }
            });
    });
}

}
