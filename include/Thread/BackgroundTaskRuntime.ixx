module;
#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module Core.Thread.BackgroundTaskRuntime;

import std;

// import Core.Define; // Module not found
// import Core.Time.Rational; // Module not found

namespace ArtifactCore {

/// <summary>
/// Background Task Runtime - Phase 1: Task Contract
///
/// render / proxy / cache warm / analysis などの非同期処理を
/// 共通の typed interface で扱うための基盤。
///
/// 責務:
/// - task id / category / priority / state の typed 管理
/// - progress reporting
/// - cancel token
/// - dependency list
/// - error payload
/// </summary>

// ============================================================
// Task Priority
// ============================================================

/// <summary>
/// Taskの優先度。値が小さいほど高優先度。
/// </summary>
export enum class TaskPriority {
  Critical = 0, // ユーザー操作に直接関係（例: 即時プレビュー）
  High = 1,     // ユーザーが待っている（例: アクティブなレンダリング）
  Normal = 2,   // 通常のバックグラウンド処理（例: proxy生成）
  Low = 3,      // 余裕があるときに（例: サムネイル生成）
  Idle = 4      // 最も低い（例: 解析、インデックス）
};

// ============================================================
// Task State
// ============================================================

/// <summary>
/// Taskの状態遷移
/// </summary>
export enum class TaskState {
  Pending,   // キュー入り待ち
  Scheduled, // スケジュール済み、実行待ち
  Running,   // 実行中
  Completed, // 完了
  Cancelled, // キャンセル済み
  Failed     // エラー
};

// ============================================================
// Task Category
// ============================================================

/// <summary>
/// Taskのカテゴリ。実行ポリシーやリソース割り当てに使用。
/// </summary>
export enum class TaskCategory {
  Render,    // レンダリング関連
  Proxy,     // Proxy生成
  CacheWarm, // キャッシュウォーム
  Analysis,  // 解析（waveform, AI, etc）
  Import,    // インポート処理
  Export,    // エクスポート処理
  Custom     // カスタム
};

// ============================================================
// Task ID
// ============================================================

/// <summary>
/// Taskの一意識別子
/// </summary>
export struct TaskId {
  uint64_t value;

  auto operator==(const TaskId &other) const -> bool = default;
  auto operator!=(const TaskId &other) const -> bool = default;
  auto operator<(const TaskId &other) const -> bool = default;

  static auto Next() -> TaskId {
    static std::atomic<uint64_t> counter{0};
    return TaskId{counter.fetch_add(1)};
  }

  static auto Invalid() -> TaskId { return TaskId{UINT64_MAX}; }

  auto IsValid() const -> bool { return value != UINT64_MAX; }
};

// ============================================================
// Task Progress
// ============================================================

/// <summary>
/// Taskの進捗状況
/// </summary>
export struct TaskProgress {
  double completed = 0.0; // 0.0 - 1.0
  double total = 1.0;     // 総量（normalized不要な場合用）
  QString
      message; // ユーザー向けメッセージ（例: "フレーム 120/300 を処理中..."）

  auto GetPercentage() const -> double {
    if (total <= 0.0)
      return 0.0;
    return std::min(100.0, (completed / total) * 100.0);
  }

  auto IsComplete() const -> bool { return completed >= total; }

  auto operator==(const TaskProgress &other) const -> bool = default;
};

// ============================================================
// Task Error
// ============================================================

/// <summary>
/// Taskのエラー情報
/// </summary>
export struct TaskError {
  int code = 0;
  QString message;
  QString details;  // スタックトレースや詳細情報
  QString category; // "render", "io", "validation", etc

  auto IsEmpty() const -> bool { return code == 0 && message.isEmpty(); }

  static auto None() -> TaskError { return TaskError{}; }

  static auto FromException(const QString &msg, const QString &cat = "unknown")
      -> TaskError {
    return TaskError{
        .code = -1, .message = msg, .details = "", .category = cat};
  }
};

// ============================================================
// Cancel Token
// ============================================================

/// <summary>
/// Taskのキャンセルを通知するトークン
/// スレッドセーフ
/// </summary>
export class CancelToken {
public:
  CancelToken() : cancelled_(false) {}
  CancelToken(const CancelToken &other) : cancelled_(other.cancelled_.load()) {}

  void RequestCancel() { cancelled_.store(true, std::memory_order_relaxed); }

  void Reset() { cancelled_.store(false, std::memory_order_relaxed); }

  auto IsCancelled() const -> bool {
    return cancelled_.load(std::memory_order_relaxed);
  }

  /// <summary>
  /// キャンセル済みであればstd::abortを投げる（task側で定期的に呼ぶ）
  /// </summary>
  void ThrowIfCancelled() const {
    if (IsCancelled()) {
      throw std::runtime_error("Task cancelled");
    }
  }

private:
  std::atomic<bool> cancelled_;
};

// ============================================================
// Task Options
// ============================================================

/// <summary>
/// Task実行時のオプション
/// </summary>
export struct TaskOptions {
  TaskPriority priority = TaskPriority::Normal;
  int maxConcurrency = 1;           // 並列実行数（0 = unlimited）
  bool allowsCancellation = true;   // キャンセル可能か
  std::vector<TaskId> dependencies; // 依存するtask id
  QString name;                     // デバッグ用task名
  QString category;                 // カテゴリ文字列
};

// ============================================================
// Task Snapshot
// ============================================================

/// <summary>
/// Taskの状態のスナップショット（UI側への通知用）
/// </summary>
export struct TaskSnapshot {
  TaskId id;
  TaskState state;
  TaskCategory category;
  TaskPriority priority;
  TaskProgress progress;
  TaskError error;
  QString name;
  std::chrono::steady_clock::time_point startTime;
  std::chrono::steady_clock::time_point endTime;

  auto GetElapsed() const -> std::chrono::milliseconds {
    if (state == TaskState::Pending || state == TaskState::Scheduled) {
      return std::chrono::milliseconds::zero();
    }
    auto end = (state == TaskState::Running) ? std::chrono::steady_clock::now()
                                             : endTime;
    return std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                 startTime);
  }
};

// ============================================================
// IBackgroundTask
// ============================================================

/// <summary>
/// Background Taskのインターフェース
///
/// 実装側は Execute() をオーバーライドし、
/// 定期的に cancelToken.ThrowIfCancelled() を呼ぶことで
/// キャンセルに対応する。
/// </summary>
export class IBackgroundTask {
public:
  virtual ~IBackgroundTask() = default;

  /// <summary>
  /// Taskの実行本体。例外を投げるとFailed扱いになる。
  /// </summary>
  virtual auto Execute(CancelToken &cancelToken,
                       std::function<void(TaskProgress)> reportProgress)
      -> void = 0;

  /// <summary>
  /// Taskのオプションを返す
  /// </summary>
  virtual auto GetOptions() const -> TaskOptions { return TaskOptions{}; }

  /// <summary>
  /// Taskの一意なID
  /// </summary>
  auto GetTaskId() const -> TaskId { return taskId_; }

  /// <summary>
  /// Taskの状態スナップショットを返す
  /// </summary>
  auto GetSnapshot() const -> TaskSnapshot { return snapshot_; }

protected:
  void UpdateSnapshot(std::function<void(TaskSnapshot &)> update) {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    update(snapshot_);
  }

private:
  TaskId taskId_ = TaskId::Next();
  TaskSnapshot snapshot_;
  mutable std::mutex snapshotMutex_;
};

// ============================================================
// TaskResult
// ============================================================

/// <summary>
/// Taskの実行結果
/// </summary>
export template <typename T> struct TaskResult {
  T value;
  TaskError error;
  bool success = true;

  static auto Success(T val) -> TaskResult {
    return TaskResult{
        .value = std::move(val), .error = TaskError::None(), .success = true};
  }

  static auto Failure(TaskError err) -> TaskResult {
    return TaskResult{.value = T{}, .error = std::move(err), .success = false};
  }

  auto IsSuccess() const -> bool { return success; }
  auto IsFailure() const -> bool { return !success; }
};

// ============================================================
// Helper: Cancel Guard
// ============================================================

/// <summary>
/// スコープ抜け時に自動的にキャンセルをリセットするガード
/// </summary>
export class CancelGuard {
public:
  explicit CancelGuard(CancelToken &token) : token_(token) {}
  ~CancelGuard() { token_.Reset(); }

  CancelGuard(const CancelGuard &) = delete;
  CancelGuard &operator=(const CancelGuard &) = delete;

  void RequestCancel() { token_.RequestCancel(); }

private:
  CancelToken &token_;
};

} // namespace ArtifactCore

// std::hash specialization for TaskId
template <> struct std::hash<ArtifactCore::TaskId> {
  size_t operator()(const ArtifactCore::TaskId &id) const noexcept {
    return std::hash<uint64_t>{}(id.value);
  }
};
