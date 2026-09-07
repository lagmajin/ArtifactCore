module;
#include <utility>
#include <cstddef>
#include <functional>
#include <future>
#include <atomic>
#include <mutex>
#include <vector>
#include <tbb/task_group.h>
#include <tbb/task_arena.h>
#include <tbb/global_control.h>
#include "../Define/DllExportMacro.hpp"
export module Core.ThreadPool;

import Memory.SharedPtr;

export namespace ArtifactCore {

    /**
     * @brief TBB task_group backed shim with the legacy ThreadPool API.
     * APIは完全維持し内部のみwork-stealing化。globalInstanceは
     * プロセス共有のtask_arena上で動作する。
     */
    class LIBRARY_DLL_API ThreadPool {
    public:
        explicit ThreadPool(size_t threads = std::thread::hardware_concurrency())
            : concurrency_(std::max<size_t>(1, threads))
            , arena_(static_cast<int>(concurrency_))
            , stop_(false) {
            control_ = std::make_unique<tbb::global_control>(
                tbb::global_control::max_allowed_parallelism, concurrency_);
        }

        ~ThreadPool() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stop_ = true;
            }
            group_.wait();
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args)
            -> std::future<typename std::invoke_result<F, Args...>::type> {

            using return_type = typename std::invoke_result<F, Args...>::type;

            auto task = makeShared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> res = task->get_future();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
            }
            arena_.execute([this, task]{
                group_.run([task]{ (*task)(); });
            });
            return res;
        }

        void enqueueTask(std::function<void()> task) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_) return;
            }
            // copy into shared_ptr to keep callable alive after move
            auto shared = makeShared<std::function<void()>>(std::move(task));
            arena_.execute([this, shared]{
                group_.run([shared]{ (*shared)(); });
            });
        }

        void waitAll() {
            group_.wait();
        }

        [[deprecated("ThreadPoolはTBB shimとして維持。DAGは Core.TaskSystem を推奨")]]
        static ThreadPool& globalInstance() {
            static ThreadPool instance;
            return instance;
        }

        size_t concurrency() const noexcept { return concurrency_; }

    private:
        size_t concurrency_;
        tbb::task_arena arena_;
        tbb::task_group group_;
        std::unique_ptr<tbb::global_control> control_;
        std::mutex mutex_;
        bool stop_;
    };
}
