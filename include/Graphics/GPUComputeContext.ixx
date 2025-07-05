module;
#include <QUuid>
#include <DiligentCore/Platforms/interface/PlatformDefinitions.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include "../Define/DllExportMacro.hpp"
export module Graphics:GPUcomputeContext;
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"DiligentCore.lib")
#pragma comment(lib,"GraphicsEngineD3D12_64d.lib")
#pragma comment(lib,"Archiver_64d.lib")
#pragma comment(lib,"spirv-cross-cored.lib")
#pragma comment(lib,"GraphicsEngineOpenGL_64d.lib")
#pragma comment(lib,"MachineIndependentd.lib")
#pragma comment(lib,"GenericCodeGend.lib")
#pragma comment(lib,"spirv-cross-cored.lib")
#pragma comment(lib,"SPIRVd.lib")


namespace Diligent {}//dummy

export namespace ArtifactCore
{
 
 using namespace Diligent;

 struct DeviceResources
 {

  RefCntAutoPtr<Diligent::IRenderDevice>  pDevice;
  RefCntAutoPtr<Diligent::IDeviceContext> pContext;
 };

 class LIBRARY_DLL_API GpuContext {
 private:
  struct Impl;
  Impl* pImpl_;
 public:
  GpuContext();
  ~GpuContext();
  void Initialize();
  RefCntAutoPtr<IRenderDevice> D3D12RenderDevice();
  RefCntAutoPtr<IDeviceContext> D3D12DeviceContext();
  DeviceResources D3D12DeviceResources();
  DeviceResources VKDeviceResources();

  RefCntAutoPtr<IShader> CompileShader(const char* shaderSource, SHADER_TYPE type, const char* entryPoint);
 private:

 };







};