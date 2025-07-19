module;
#include <QObject>
#include <QThread>
#include <QRunnable>
export module Render.Worker;



export namespace ArtifactCore {
 
 
 enum class RenderJobStatus {
  Pending,     // 準備中/待機中
  Queued,      // キューに入っているが、まだ処理されていない
  Rendering,   // レンダリング中
  Paused,      // 一時停止中
  Completed,   // 完了
  Failed,      // 失敗
  Cancelled    // キャンセル済み
 };

 class RenderWorker :public QRunnable{
 private:
  class Impl;
 public:
  RenderWorker();
  ~RenderWorker();
  void run() override;

 };









};