module;
#include <utility>

#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Fence.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround)

export module Graphics.Fence.Tracker;

namespace Diligent{}

export namespace ArtifactCore {

 using namespace Diligent;

 class FenceTracker
 {
 private:
  IFence* fence_ = nullptr;
  Uint64 currentValue_ = 0;

 public:
  FenceTracker(IRenderDevice* device, const char* name = "Fence")
  {
   FenceDesc desc;
   desc.Name = name;
   device->CreateFence(desc, &fence_);
   // CreateFence returns with ref=1; no extra AddRef needed
  }

  ~FenceTracker()
  {
   if (fence_) { fence_->Release(); fence_ = nullptr; }
  }

  FenceTracker(const FenceTracker&) = delete;
  FenceTracker& operator=(const FenceTracker&) = delete;

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
