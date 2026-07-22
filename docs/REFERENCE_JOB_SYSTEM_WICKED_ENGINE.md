# Job System Design Reference (from Wicked Engine wiJobSystem.h)

> Source: https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiJobSystem.h
> Target: C-ARC-6 Typed Background Task Runtime

## API Design

```cpp
namespace ArtifactCore::jobsystem {

void Initialize(uint32_t maxThreadCount = ~0u);
void ShutDown();
bool IsShuttingDown();

struct JobArgs {
    uint32_t jobIndex;       // SV_DispatchThreadID 
    uint32_t groupID;        // SV_GroupID
    uint32_t groupIndex;     // SV_GroupIndex
    bool isFirstJobInGroup;
    bool isLastJobInGroup;
    void* sharedmemory;      // グループ内共有スタック (64-byte aligned)
};

enum class Priority { High, Low, Streaming, Count };

struct context {
    std::atomic<uint32_t> counter{ 0 };
    Priority priority = Priority::High;
};

uint32_t GetThreadCount(Priority priority = Priority::High);

// 単一タスクの非同期実行
void Execute(context& ctx, const function<void(JobArgs)>& task);

// 並列タスク実行
//   jobCount  : 総ジョブ数
//   groupSize : 1スレッドが逐次実行するジョブ数（小ジョブ向けのバッチ化）
//   task      : JobArgs を受け取るコールバック
//   sharedmemory_size : 各グループの共有スタックサイズ
void Dispatch(context& ctx, uint32_t jobCount, uint32_t groupSize,
              const function<void(JobArgs)>& task, size_t sharedmemory_size = 0);

uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize);
bool IsBusy(const context& ctx);

// 完了待ち（呼び出し元スレッドもワーカー化してジョブを消化）
void Wait(const context& ctx);

uint32_t GetRemainingJobCount(const context& ctx);

} // namespace ArtifactCore::jobsystem
```

## Key Design Decisions

1. **context が実行状態を持つ** — 複数の context を同時に使い分け可能
2. **Wait() は呼び出し元もワーカー化** — デッドロック防止＋CPU 使用率最大化
3. **Priority は 3 段階**:
   - `High` : レンダリング、メインスレッドワーク（全コア）
   - `Low` : 汎用バックグラウンドタスク（専用スレッドプール）
   - `Streaming` : アセットストリーミング（単一低優先スレッド）
4. **sharedmemory** : Dispatch のグループ内で共有されるスタックメモリ。
   64-byte aligned。グループ内ジョブは逐次実行なのでロック不要。
5. **IsShuttingDown()** : 長時間ジョブはこれをチェックして早期脱出すべき

## ArtifactCore Adaptations

- `wi::function<void(JobArgs), 96>` → `std::function<void(JobArgs)>` で十分
  （96バイト制限はアロケーション回避の最適化。最初はシンプルに）
- `Fixed-size function` は後日検討
- `Priority::Streaming` は VideoLayer のプロキシ生成や texture 遅延ロードに割り当て
- preview render / export のフレーム並列化に `Dispatch` を使用

## Implementation Notes

- スレッドプールは `std::thread` + ワークスティーリングキュー
- `context::counter` を atomic fetch_add / fetch_sub で管理
- `Wait()` 内で while(counter > 0) { try_pop_and_execute(); }
- ワーカースレッドが空のときは `std::this_thread::yield()` ではなく
  `WaitOnAddress` / `futex` で待機（省電力）

## Usage Examples

```cpp
// 例1: 単一バックグラウンドタスク
jobsystem::context ctx;
jobsystem::Execute(ctx, [](jobsystem::JobArgs args) {
    heavyComputation();
});
// ... 他の処理 ...
jobsystem::Wait(ctx);  // 完了保証

// 例2: フレームの並列レンダリング
jobsystem::context ctx;
jobsystem::Dispatch(ctx, frameCount, 1, [&](jobsystem::JobArgs args) {
    renderFrame(args.jobIndex);
});
jobsystem::Wait(ctx);
```