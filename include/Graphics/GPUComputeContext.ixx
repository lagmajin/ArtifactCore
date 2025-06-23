

module;
#include <DiligentCore/Platforms/interface/PlatformDefinitions.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
export module Graphics:GPUcomputeContext;



export namespace ArtifactCore
{

 using namespace Diligent;

 class GpuContext {
 public:
  GpuContext();
  ~GpuContext();

  void Initialize(); // Diligent�̏�����

  // �A�N�Z�T
  Diligent::IRenderDevice* GetDevice() const { return pDevice_; }
  Diligent::IDeviceContext* GetContext() const { return pContext_; }

 private:
  RefCntAutoPtr<Diligent::IRenderDevice> pDevice_;
  RefCntAutoPtr<Diligent::IDeviceContext> pContext_;
  RefCntAutoPtr<Diligent::ISwapChain> pSwapChain_; // �s�g�p�Ȃ�nullptr��OK
 };







};