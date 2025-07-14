module;
export module GPUTaskHandle;

export namespace ArtifactCore {

 class GPUTaskHandle
 {
 public:
  // このタスクがGPUで完了したかを確認
  bool IsCompleted() const;

  // このタスクの完了を待機 (CPUがブロックされる)
  void WaitUntilCompleted();

  // このタスクの結果として生成されたGPUTextureを取得
  // ※ 注意: タスクが完了していない場合、結果は未定義か、待機するか、エラーを発生させる実装
  GPUTexture GetResultTexture() const;

 private:
  // 内部的なGPUフェンス、イベント、関連するコマンドリストや結果リソースへのポインタなど
  friend class GPUTaskScheduler; // スケジューラーが内部状態を操作できるようにする
 };









};