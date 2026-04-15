export module Core.Thread.BackgroundTaskWorkerPool;

import std;
import Core.Thread.BackgroundTaskRuntime;
// import Core.Event.EventBus; // Module not found

namespace ArtifactCore {

// ============================================================
// Task Events
// ============================================================

/// <summary>
/// Taskの状態変更イベント
/// </summary>
export struct TaskStateChangedEvent {
  TaskId taskId;
  TaskState oldState;
  TaskState newState;
  TaskSnapshot snapshot;
};

/// <summary>
/// Task進捗更新イベント
/// </summary>
export struct TaskProgressEvent {
  TaskId taskId;
  TaskProgress progress;
};

/// <summary>
/// Task完了イベント
/// </summary>
export struct TaskCompletedEvent {
  TaskId taskId;
  TaskSnapshot snapshot;
};

/// <summary>
/// Task失敗イベント
/// </summary>
export struct TaskFailedEvent {
  TaskId taskId;
  TaskError error;
  TaskSnapshot snapshot;
};

// ============================================================
// BackgroundTaskWorkerPool
// ============================================================

/// <summary>
/// Background Task Worker Pool - Phase 2
///
/// 複数のtaskを並列実行し、優先度ベースでスケジューリングする。
///
/// 責務:
/// - taskのキュー管理
/// - worker threadのプール
/// - 優先度ベースのスケジューリング
/// - 依存関係の解決
/// - 同時実行数の制限
/// - 結果のdispatch
/// </summary>
export class BackgroundTaskWorkerPool {
public:
  struct Config {
    int maxWorkers = static_cast<int>(
        std::thread::hardware_concurrency()); // デフォルトは論理コア数
    int maxPendingTasks = 1000;               // キューの最大長
    bool enableDependencyResolution = true;   // 依存関係の自動解決
  };

  explicit BackgroundTaskWorkerPool(Config config = {})
      : config_(config), running_(false) {

    if (config_.maxWorkers <= 0) {
      config_.maxWorkers =
          std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    }
  }

  ~BackgroundTaskWorkerPool() { Shutdown(); }

  /// <summary>
  /// Worker poolを開始する
  /// </summary>
  void Start() {
    if (running_.load()) {
      return;
    }

    running_.store(true);

    // Worker threadsを起動
    for (int i = 0; i < config_.maxWorkers; ++i) {
      workers_.emplace_back(&BackgroundTaskWorkerPool::WorkerLoop, this, i);
    }

    // Scheduler threadを起動
    schedulerThread_ =
        std::thread(&BackgroundTaskWorkerPool::SchedulerLoop, this);
  }

  /// <summary>
  /// Worker poolをシャットダウンする
  /// </summary>
  void Shutdown() {
    if (!running_.load()) {
      return;
    }

    running_.store(false);
    queueCV_.notify_all();

    for (auto &worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }

    if (schedulerThread_.joinable()) {
      schedulerThread_.join();
    }

    workers_.clear();
  }

  /// <summary>
  /// Taskをキューに追加する
  /// </summary>
  auto SubmitTask(std::shared_ptr<IBackgroundTask> task) -> TaskId {
    std::lock_guard<std::mutex> lock(queueMutex_);

    if (pendingTasks_.size() >= config_.maxPendingTasks) {
      throw std::runtime_error("Task queue is full");
    }

    TaskId id = task->GetTaskId();
    pendingTasks_.push_back(std::move(task));

    // EventBusに通知
    TaskStateChangedEvent event{.taskId = id,
                                .oldState = TaskState::Pending,
                                .newState = TaskState::Pending,
                                .snapshot = {}};
    // if (eventBus_) {
    //     eventBus_->Publish(event);
    // } // EventBus module missing

    queueCV_.notify_one();
    return id;
  }

  /// <summary>
  /// Taskをキャンセルする
  /// </summary>
  void CancelTask(TaskId taskId) {
    std::lock_guard<std::mutex> lock(cancelMutex_);

    if (auto it = cancelTokens_.find(taskId); it != cancelTokens_.end()) {
      it->second.RequestCancel();
    }
  }

  /// <summary>
  /// 特定カテゴリのtaskをすべてキャンセルする
  /// </summary>
  void CancelTasksByCategory(TaskCategory category) {
    std::lock_guard<std::mutex> lock(cancelMutex_);

    for (auto &[id, token] : cancelTokens_) {
      auto snapshot = GetTaskSnapshot(id);
      if (snapshot.category == category) {
        token.RequestCancel();
      }
    }
  }

  /// <summary>
  /// 特定のtaskのスナップショットを取得する
  /// </summary>
  auto GetTaskSnapshot(TaskId taskId) const -> TaskSnapshot {
    std::lock_guard<std::mutex> lock(snapshotsMutex_);
    if (auto it = snapshots_.find(taskId); it != snapshots_.end()) {
      return it->second;
    }
    return TaskSnapshot{};
  }

  /// <summary>
  /// すべてのtaskのスナップショットを取得する
  /// </summary>
  auto GetAllSnapshots() const -> std::vector<TaskSnapshot> {
    std::lock_guard<std::mutex> lock(snapshotsMutex_);
    std::vector<TaskSnapshot> result;
    result.reserve(snapshots_.size());
    for (const auto &[id, snapshot] : snapshots_) {
      result.push_back(snapshot);
    }
    return result;
  }

  /// <summary>
  /// EventBusを設定する（UI通知用）
  /// </summary>
  void SetEventBus(std::shared_ptr<int> eventBus) {
    // eventBus_ = std::move(eventBus); // EventBus module missing
  }

  /// <summary>
  /// 実行中のtask数を取得する
  /// </summary>
  auto GetRunningCount() const -> int {
    std::lock_guard<std::mutex> lock(snapshotsMutex_);
    int count = 0;
    for (const auto &[id, snapshot] : snapshots_) {
      if (snapshot.state == TaskState::Running) {
        ++count;
      }
    }
    return count;
  }

  /// <summary>
  /// キュー待ちのtask数を取得する
  /// </summary>
  auto GetPendingCount() const -> int {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return static_cast<int>(pendingTasks_.size());
  }

  /// <summary>
  /// キューをクリアする（実行中はキャンセル）
  /// </summary>
  void ClearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    for (auto &task : pendingTasks_) {
      CancelTask(task->GetTaskId());
    }
    pendingTasks_.clear();
  }

private:
  struct PrioritizedTask {
    TaskPriority priority;
    std::shared_ptr<IBackgroundTask> task;

    auto operator<(const PrioritizedTask &other) const -> bool {
      return priority > other.priority; // 値が小さいほど高優先度
    }
  };

  /// <summary>
  /// Worker threadのメインループ
  /// </summary>
  void WorkerLoop(int workerId) {
    while (running_.load()) {
      std::shared_ptr<IBackgroundTask> task;

      // Taskを取得
      {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCV_.wait(lock, [this] {
          return !pendingTasks_.empty() || !running_.load();
        });

        if (!running_.load()) {
          return;
        }

        // 優先度でソートして最も高いものを取り出す
        std::sort(pendingTasks_.begin(), pendingTasks_.end(),
                  [](const auto &a, const auto &b) {
                    return a->GetOptions().priority < b->GetOptions().priority;
                  });

        task = std::move(pendingTasks_.front());
        pendingTasks_.pop_front();
      }

      if (!task) {
        continue;
      }

      // Taskを実行
      ExecuteTask(task, workerId);
    }
  }

  /// <summary>
  /// Scheduler threadのメインループ（依存関係の解決）
  /// </summary>
  void SchedulerLoop() {
    while (running_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (config_.enableDependencyResolution) {
        ResolveDependencies();
      }
    }
  }

  /// <summary>
  /// Taskを実行する
  /// </summary>
  void ExecuteTask(std::shared_ptr<IBackgroundTask> task, int workerId) {
    TaskId taskId = task->GetTaskId();
    TaskState oldState = TaskState::Scheduled;
    TaskState newState = TaskState::Running;

    // CancelTokenを登録
    CancelToken cancelToken;
    {
      std::lock_guard<std::mutex> lock(cancelMutex_);
      cancelTokens_.emplace(taskId, std::move(cancelToken));
    }

    // Snapshot更新
    TaskSnapshot snapshot{.id = taskId,
                          .state = TaskState::Running,
                          .category =
                              task->GetOptions().priority != TaskPriority::Idle
                                  ? TaskCategory::Custom
                                  : TaskCategory::Custom,
                          .priority = task->GetOptions().priority,
                          .progress = {},
                          .error = TaskError::None(),
                          .name = task->GetOptions().name,
                          .startTime = std::chrono::steady_clock::now(),
                          .endTime = {}};

    {
      std::lock_guard<std::mutex> lock(snapshotsMutex_);
      snapshots_[taskId] = snapshot;
    }

    // EventBusに通知
    TaskStateChangedEvent stateEvent{.taskId = taskId,
                                     .oldState = oldState,
                                     .newState = newState,
                                     .snapshot = snapshot};
    // if (eventBus_) {
    //     eventBus_->Publish(stateEvent);
    // }

    // Task実行
    try {
      auto reportProgress = [this, taskId](TaskProgress progress) {
        TaskProgressEvent event{.taskId = taskId, .progress = progress};
        // if (eventBus_) {
        //     eventBus_->Publish(event);
        // }

        std::lock_guard<std::mutex> lock(snapshotsMutex_);
        if (auto it = snapshots_.find(taskId); it != snapshots_.end()) {
          it->second.progress = progress;
        }
      };

      task->Execute(cancelToken, reportProgress);

      // 完了
      newState = TaskState::Completed;
      snapshot.state = TaskState::Completed;
      snapshot.endTime = std::chrono::steady_clock::now();

      {
        std::lock_guard<std::mutex> lock(snapshotsMutex_);
        snapshots_[taskId] = snapshot;
      }

      TaskCompletedEvent completedEvent{.taskId = taskId, .snapshot = snapshot};
      // if (eventBus_) {
      //     eventBus_->Publish(completedEvent);
      // }

    } catch (const std::exception &e) {
      // キャンセルによる例外
      if (cancelToken.IsCancelled()) {
        newState = TaskState::Cancelled;
        snapshot.state = TaskState::Cancelled;
      } else {
        // その他のエラー
        newState = TaskState::Failed;
        snapshot.state = TaskState::Failed;
        snapshot.error = TaskError::FromException(
            QString::fromStdString(e.what()), "runtime");
      }

      snapshot.endTime = std::chrono::steady_clock::now();

      {
        std::lock_guard<std::mutex> lock(snapshotsMutex_);
        snapshots_[taskId] = snapshot;
      }

      if (snapshot.state == TaskState::Failed) {
        TaskFailedEvent failedEvent{
            .taskId = taskId, .error = snapshot.error, .snapshot = snapshot};
        // if (eventBus_) {
        //     eventBus_->Publish(failedEvent);
        // }
      }
    }

    // 状態変更イベント
    TaskStateChangedEvent finalStateEvent{.taskId = taskId,
                                          .oldState = oldState,
                                          .newState = newState,
                                          .snapshot = snapshot};
    // if (eventBus_) {
    //     eventBus_->Publish(finalStateEvent);
    // }

    // CancelTokenを削除
    {
      std::lock_guard<std::mutex> lock(cancelMutex_);
      cancelTokens_.erase(taskId);
    }
  }

  /// <summary>
  /// 依存関係を解決する
  /// </summary>
  void ResolveDependencies() {
    std::lock_guard<std::mutex> lock(queueMutex_);

    for (auto it = pendingTasks_.begin(); it != pendingTasks_.end();) {
      const auto &task = *it;
      const auto &deps = task->GetOptions().dependencies;

      // すべての依存taskが完了しているか確認
      bool allDepsCompleted = true;
      for (TaskId depId : deps) {
        auto depSnapshot = GetTaskSnapshot(depId);
        if (depSnapshot.state != TaskState::Completed) {
          allDepsCompleted = false;
          break;
        }
      }

      if (allDepsCompleted) {
        // 依存関係満たされた、実行可能
        ++it;
      } else {
        // まだ依存関係が満たされていない、キューの後ろに移動
        auto task = std::move(*it);
        it = pendingTasks_.erase(it);
        pendingTasks_.push_back(std::move(task));
      }
    }
  }

  Config config_;
  std::atomic<bool> running_;

  std::vector<std::thread> workers_;
  std::thread schedulerThread_;

  std::deque<std::shared_ptr<IBackgroundTask>> pendingTasks_;
  mutable std::mutex queueMutex_;
  std::condition_variable queueCV_;

  std::unordered_map<TaskId, CancelToken> cancelTokens_;
  std::mutex cancelMutex_;

  std::unordered_map<TaskId, TaskSnapshot> snapshots_;
  mutable std::mutex snapshotsMutex_;

  // std::shared_ptr<EventBus> eventBus_; // EventBus module missing
};

} // namespace ArtifactCore
