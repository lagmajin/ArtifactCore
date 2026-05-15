module;
#include <utility>
#include <QUuid>
#include <DiligentCore/Platforms/interface/PlatformDefinitions.h>
// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround: stop_token specialization conflict)
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include "../Define/DllExportMacro.hpp"
export module Graphics.GPUcomputeContext;

import Graphics.GPU.Info;

// #pragma comment(lib,"d3d12.lib")
// #pragma comment(lib,"d3dcompiler.lib")
// #pragma comment(lib,"dxguid.lib")
// #pragma comment(lib,"dxgi.lib")
// #pragma comment(lib,"DiligentCore.lib")
// #pragma comment(lib,"GraphicsEngineD3D12_64d.lib")
// #pragma comment(lib,"Archiver_64d.lib")
// #pragma comment(lib,"spirv-cross-cored.lib")
// #pragma comment(lib,"GraphicsEngineOpenGL_64d.lib")
// #pragma comment(lib,"MachineIndependentd.lib")
// #pragma comment(lib,"GenericCodeGend.lib")
// #pragma comment(lib,"spirv-cross-cored.lib")
// #pragma comment(lib,"SPIRVd.lib")


namespace Diligent {}//dummy

export namespace ArtifactCore
{
 
 using namespace Diligent;

 // Raw-pointer view of device resources.
 // Callers that need ownership should hold their own RefCntAutoPtr.
 struct DeviceResources
 {
  IRenderDevice*  pDevice  = nullptr;
  IDeviceContext* pContext = nullptr;
 };

 class LIBRARY_DLL_API GpuContext {
 private:
  struct Impl;
  Impl* pImpl_;
 public:
  GpuContext();
  // Accepts raw pointers; internally stores as RefCntAutoPtr (AddRef is called)
  GpuContext(IRenderDevice* device, IDeviceContext* context);
  ~GpuContext();
  void Initialize();
  IRenderDevice* RenderDevice();
  IDeviceContext* DeviceContext();
  IRenderDevice*  D3D12RenderDevice();
  IDeviceContext* D3D12DeviceContext();
  DeviceResources D3D12DeviceResources();
  DeviceResources VKDeviceResources();
  GPUInfo info() const;
  
  // Output-parameter style: *ppShader has ref-count == 1 (caller must Release or wrap in RefCntAutoPtr)
  bool CompileShader(const char* shaderSource, SHADER_TYPE type, const char* entryPoint, IShader** ppShader);
 private:

 };







};
