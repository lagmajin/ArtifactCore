

module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/Fence.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
export module Graphics.Fence.Tracker;

namespace Diligent{}

export namespace ArtifactCore {

 using namespace Diligent;

 class FenceTracker
 {
 private:
  RefCntAutoPtr<IFence> fence_;
  Uint64 currentValue_ = 0;

 public:
  FenceTracker(IRenderDevice* device, const char* name = "Fence")
  {
   FenceDesc desc;
   desc.Name = name;
   device->CreateFence(desc, &fence_);
  }

  // GPU に Signal
  void Signal(IDeviceContext* context)
  {
   ++currentValue_;
   context->EnqueueSignal(fence_, currentValue_);
  }

  // 完了したか確認
  bool IsSignaled() const
  {
   return fence_->GetCompletedValue() >= currentValue_;
  }

  // GPUの完了を待機
  void Wait() const
  {
   fence_->Wait(currentValue_);
  }

  // フェンスの現在値取得（デバッグ用）
  Uint64 CurrentValue() const { return currentValue_; }
 };








};