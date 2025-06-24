

module;
#include <DiligentCore/Platforms/interface/PlatformDefinitions.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
export module Graphics:GPUcomputeContext;
#pragma comment(lib,"DiligentCore.lib")
#pragma comment(lib,"GraphicsEngineD3D12_64d.lib")

#pragma comment(lib,"GraphicsEngineOpenGL_64d.lib")

namespace Diligent {}//dummy

export namespace ArtifactCore
{
 
 using namespace Diligent;



 class GpuContext {
 private:
  struct Impl;
  Impl* pImpl_;
 public:
  GpuContext();
  ~GpuContext();
  void Initialize();
  RefCntAutoPtr<IRenderDevice> D3D12RenderDevice();
  RefCntAutoPtr<IShader> CompileShader(const char* shaderSource, SHADER_TYPE type, const char* entryPoint);
 private:

 };







};