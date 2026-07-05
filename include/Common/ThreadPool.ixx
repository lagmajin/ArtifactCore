module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
export module Core.ThreadPool;

import std;

export namespace ArtifactCore {

    /**
     * @brief 軽量で高速なタスク並列処理（DAG評価など）のためのスレッドプール
     * std::async の毎タスクごとのスレッド生成オーバーヘッドをなくします。
     */
    class LIBRARY_DLL_API ThreadPool {
    public:
        // ハードウェアの並行性に合わせてデフォルトスレッド数を決定
        explicit ThreadPool(size_t threads = std::thread::hardware_concurrency())
            : stop_(false), active_tasks_(0) {
            for (size_t i = 0; i < std::max<size_t>(1, threads); ++i) {
                workers_.emplace_back([this] {
                    for (;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex_);
                            this->condition_.wait(lock, [this] { 
                                return this->stop_ || !this->tasks_.empty(); 
                            });
                            
                            if (this->stop_ && this->tasks_.empty())
                                return;
                            
                            task = std::move(this->tasks_.front());
                            this->tasks_.pop();
                        }
                        
                        ++active_tasks_;
                        task(); // タスクの実行
                        --active_tasks_;
                        
                        // タスク完了時に待機中スレッドに通知
                        wait_condition_.notify_all();
                    }
                });
            }
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                stop_ = true;
            }
            condition_.notify_all();
            for (std::thread &worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        /**
         * @brief 非同期タスクをキューに追加
         */
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) 
            -> std::future<typename std::invoke_result<F, Args...>::type> {
            
            using return_type = typename std::invoke_result<F, Args...>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
                
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (stop_) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
                tasks_.emplace([task]() { (*task)(); });
            }
            condition_.notify_one();
            return res;
        }

        /**
         * @brief ワーカーに直接 std::function(void) を追加する（軽量版）
         */
        void enqueueTask(std::function<void()> task) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (stop_) return;
                tasks_.emplace(std::move(task));
            }
            condition_.notify_one();
        }

        /**
         * @brief すべてのタスクが完了するまで待機する
         */
        void waitAll() {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            wait_condition_.wait(lock, [this] {
                return tasks_.empty() && active_tasks_ == 0;
            });
        }

        // シングルトンとしてのグローバルなスレッドプール
        static ThreadPool& globalInstance() {
            static ThreadPool instance;
            return instance;
        }

    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::condition_variable wait_condition_;
        bool stop_;
        std::atomic<int> active_tasks_;
    };
}
