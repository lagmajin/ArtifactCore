module;
#include <functional>
#include <future>
#include <vector>
#include <cstddef>
#include <thread>
#include <QObject>
#include <QMetaObject>
#include <QPointer>
#include <taskflow/taskflow.hpp>
#include "../Define/DllExportMacro.hpp"
export module Core.TaskSystem;

export namespace ArtifactCore {

    /**
     * @brief Taskflow work-stealing executorの薄いラッパ。
     * TBBのデータ並列(Parallel::For)に対し、DAG/非同期グラフを担当。
     * header-onlyのためABI影響なし。ThreadPoolとはスレッドプールを共有しない
     * が、concurrencyはhardware_concurrency()で統一し過剰生成を避ける。
     */
    class LIBRARY_DLL_API TaskSystem {
    public:
        explicit TaskSystem(size_t threads = std::thread::hardware_concurrency())
            : executor_(std::max<size_t>(1, threads)) {}

        ~TaskSystem() { executor_.wait_for_all(); }

        TaskSystem(const TaskSystem&) = delete;
        TaskSystem& operator=(const TaskSystem&) = delete;

        // 単一タスクを非同期投入
        template<class F>
        auto async(F&& f) -> std::future<std::invoke_result_t<F>> {
            return executor_.async(std::forward<F>(f));
        }

        void silent_async(std::function<void()> f) {
            executor_.silent_async(std::move(f));
        }

        // Taskflowグラフを実行
        tf::Future<void> run(tf::Taskflow& flow) { return executor_.run(flow); }
        tf::Future<void> run(tf::Taskflow&& flow) { return executor_.run(std::move(flow)); }

        // ワーカー内で協調実行 (corun) - デッドロック回避
        template<class T>
        void corun(T& target) { executor_.corun(target); }

        void wait_for_all() { executor_.wait_for_all(); }

        size_t concurrency() const noexcept { return executor_.num_workers(); }
        size_t num_topologies() const noexcept { return executor_.num_topologies(); }

        static TaskSystem& globalInstance() {
            static TaskSystem instance;
            return instance;
        }

        tf::Executor& native() noexcept { return executor_; }
        const tf::Executor& native() const noexcept { return executor_; }

    private:
        tf::Executor executor_;
    };

    // Qtスレッドへ結果をQueuedで返すヘルパ。QFutureWatcherの代替。
    template<class T>
    inline void asyncPostToObject(QObject* context,
                                  std::function<T()> work,
                                  std::function<void(T)> onFinished) {
        if (!context || !work || !onFinished) return;
        QPointer<QObject> guard(context);
        TaskSystem::globalInstance().silent_async([guard, work = std::move(work), onFinished = std::move(onFinished)]() mutable {
            T result{};
            try { result = work(); } catch (...) {}
            if (!guard) return;
            QMetaObject::invokeMethod(guard, [guard, onFinished = std::move(onFinished), result = std::move(result)]() mutable {
                if (!guard) return;
                onFinished(std::move(result));
            }, Qt::QueuedConnection);
        });
    }

    inline void asyncPostToObject(QObject* context,
                                  std::function<void()> work,
                                  std::function<void()> onFinished = nullptr) {
        if (!context || !work) return;
        QPointer<QObject> guard(context);
        TaskSystem::globalInstance().silent_async([guard, work = std::move(work), onFinished = std::move(onFinished)]() mutable {
            try { work(); } catch (...) {}
            if (!guard) return;
            QMetaObject::invokeMethod(guard, [guard, onFinished = std::move(onFinished)]() mutable {
                if (!guard) return;
                if (onFinished) onFinished();
            }, Qt::QueuedConnection);
        });
    }

}
