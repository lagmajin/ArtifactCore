module;

#include <QThreadPool>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>

export module Core.Thread.LightweightTask;

import Thread.Helper;

namespace ArtifactCore {

export class LightweightTaskContext;

export using LightweightTask =
    std::function<void(const LightweightTaskContext &)>;
export using LightweightIndexedTask =
    std::function<void(const LightweightTaskContext &, std::uint32_t)>;

/// A small, bounded-lifetime task group for short background work.
///
/// Tasks are submitted to the shared background QThreadPool. The context does
/// not own a thread or a queue, so it is intended for a small burst of work:
/// create a context, execute/dispatch tasks, and wait before destroying it.
/// Do not call wait() from one of the submitted tasks.
export class LightweightTaskContext {
public:
  LightweightTaskContext() = default;

  LightweightTaskContext(const LightweightTaskContext &) = delete;
  LightweightTaskContext &operator=(const LightweightTaskContext &) = delete;

  void requestCancel() noexcept {
    cancelled_.store(true, std::memory_order_release);
  }

  bool isCancelled() const noexcept {
    return cancelled_.load(std::memory_order_acquire);
  }

  bool isBusy() const noexcept {
    return pending_.load(std::memory_order_acquire) != 0;
  }

  std::uint32_t remainingTaskCount() const noexcept {
    return pending_.load(std::memory_order_acquire);
  }

  bool hasFailure() const noexcept {
    return failed_.load(std::memory_order_acquire);
  }

  /// Reuse the context after all previously submitted tasks have completed.
  bool reset() noexcept {
    if (isBusy()) {
      return false;
    }
    cancelled_.store(false, std::memory_order_release);
    failed_.store(false, std::memory_order_release);
    return true;
  }

  void wait() const {
    std::unique_lock<std::mutex> lock(waitMutex_);
    waitCondition_.wait(lock, [this] {
      return pending_.load(std::memory_order_acquire) == 0;
    });
  }

private:
  friend void executeLightweightTask(LightweightTaskContext &,
                                     LightweightTask);
  friend void dispatchLightweightTasks(LightweightTaskContext &,
                                       std::uint32_t,
                                       LightweightIndexedTask);

  void beginTask() noexcept {
    pending_.fetch_add(1, std::memory_order_acq_rel);
  }

  void finishTask(bool failed) noexcept {
    if (failed) {
      failed_.store(true, std::memory_order_release);
    }
    if (pending_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      waitCondition_.notify_all();
    }
  }

  std::atomic<std::uint32_t> pending_{0};
  std::atomic<bool> cancelled_{false};
  std::atomic<bool> failed_{false};
  mutable std::mutex waitMutex_;
  mutable std::condition_variable waitCondition_;
};

/// Submit one short task to the shared background pool.
export inline void executeLightweightTask(LightweightTaskContext &context,
                                           LightweightTask task) {
  if (!task || context.isCancelled()) {
    return;
  }

  context.beginTask();
  try {
    sharedBackgroundThreadPool().start(
        [&context, task = std::move(task)]() mutable {
          bool failed = false;
          try {
            if (!context.isCancelled()) {
              task(context);
            }
          } catch (...) {
            failed = true;
          }
          context.finishTask(failed);
        });
  } catch (...) {
    context.finishTask(true);
  }
}

/// Submit indexed short tasks without allocating a task collection.
export inline void dispatchLightweightTasks(LightweightTaskContext &context,
                                             std::uint32_t count,
                                             LightweightIndexedTask task) {
  if (!task || context.isCancelled()) {
    return;
  }

  for (std::uint32_t index = 0; index < count; ++index) {
    executeLightweightTask(
        context, [task, index](const LightweightTaskContext &taskContext) {
          task(taskContext, index);
        });
    if (context.isCancelled()) {
      break;
    }
  }
}

} // namespace ArtifactCore
