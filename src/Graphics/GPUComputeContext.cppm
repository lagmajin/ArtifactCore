module;
#include <utility>

#include <vulkan/vulkan.h>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>

#include <QSysInfo>
#include <QOperatingSystemVersion>
#include <QUuid>
module Graphics.GPUcomputeContext;

namespace Diligent {}//dummy


namespace ArtifactCore
{
 using namespace Diligent;

 struct GpuContext::Impl
 {
  QUuid	dx12id;
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pContext;

  GPUInfo info() const
  {
   QString osType = QSysInfo::productType(); // "windows", "osx", "linux" など

   auto result=GPUInfo();

   auto adapter = GraphicsAdapterInfo();
  	
   if (osType == "windows")
   {
    if (pDevice)
    {
     adapter = pDevice->GetAdapterInfo();
    
    }
   }
   else if (osType == "linux")
   {
    if (pDevice)
    {
     adapter = pDevice->GetAdapterInfo();
     // info.name = adapter.Description;
    }
   }

   
  	

   return result;
  }
 	

  void Initialize();

  RefCntAutoPtr<Diligent::IShader> CompileShader(const char* shaderSource, Diligent::SHADER_TYPE type, const char* entryPoint);



 };

 RefCntAutoPtr<Diligent::IShader> GpuContext::Impl::CompileShader(const char* shaderSource, Diligent::SHADER_TYPE type, const char* entryPoint)
 {
 if (pDevice == nullptr || shaderSource == nullptr || entryPoint == nullptr)
 {
   return {};
 }

  Diligent::ShaderCreateInfo shaderCI;
  shaderCI.Desc.ShaderType = type;
  shaderCI.Desc.Name = "MyShader";
  shaderCI.EntryPoint = entryPoint;
  shaderCI.Source = shaderSource;
  shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  // Mesh/amplification shaders have no Shader Model 5.x profile.  Leaving
  // these types on Diligent's legacy default makes it request ms_5_1, which
  // the HLSL compiler correctly rejects before a mesh PSO can be created.
  if (type == Diligent::SHADER_TYPE_MESH ||
      type == Diligent::SHADER_TYPE_AMPLIFICATION) {
   shaderCI.ShaderCompiler = Diligent::SHADER_COMPILER_DXC;
   shaderCI.HLSLVersion = {6, 5};
  }
  //shaderCI.UseCombinedTextureSamplers = true;

  Diligent::RefCntAutoPtr<Diligent::IShader> shader;
  pDevice->CreateShader(shaderCI, &shader);
  return shader;
 }

 void GpuContext::Impl::Initialize()
 {
#if VULKAN_SUPPORTED
  EngineVkCreateInfo engineVKCI;

  auto pVKFactory = GetEngineFactoryVk();
  if (!pVKFactory)
  {
   return;
  }

  pVKFactory->CreateDeviceAndContextsVk(engineVKCI, &pDevice, &pContext);
#endif
 }

 GpuContext::GpuContext() :pImpl_(new Impl())
 {

 }

 GpuContext::GpuContext(IRenderDevice* device, IDeviceContext* context)
  : pImpl_(new Impl())
 {
  pImpl_->pDevice = device;   // RefCntAutoPtr assignment → AddRef
  pImpl_->pContext = context;
 }

 GpuContext::~GpuContext()
 {
  delete pImpl_;
 }


 bool GpuContext::CompileShader(const char* shaderSource, Diligent::SHADER_TYPE type, const char* entryPoint, IShader** ppShader)
 {
  if (!ppShader) return false;
  auto shader = pImpl_->CompileShader(shaderSource, type, entryPoint);
  *ppShader = shader.Detach();
  return *ppShader != nullptr;
 }

 void GpuContext::Initialize()
 {
  pImpl_->Initialize();
 }

 IRenderDevice* GpuContext::RenderDevice()
 {
  return pImpl_->pDevice;
 }

 IDeviceContext* GpuContext::DeviceContext()
 {
  return pImpl_->pContext;
 }

 GPUCapabilitySnapshot GpuContext::capabilities() const
 {
  GPUCapabilitySnapshot result;
  if (!pImpl_ || !pImpl_->pDevice) {
   return result;
  }

  const auto& adapter = pImpl_->pDevice->GetAdapterInfo();
  result.indirectCount =
      (adapter.DrawCommand.CapFlags & DRAW_COMMAND_CAP_FLAG_DRAW_INDIRECT) != 0;
  result.descriptorIndexing =
      pImpl_->pDevice->GetDeviceInfo().Features.BindlessResources !=
      DEVICE_FEATURE_STATE_DISABLED;
  result.memory.budgetBytes = adapter.Memory.LocalMemory + adapter.Memory.UnifiedMemory;
  result.memory.valid = result.memory.budgetBytes > 0;
  result.memoryBudget = result.memory.valid;
  return result;
 }

 IRenderDevice* GpuContext::D3D12RenderDevice()
 {
  return RenderDevice();  // retained for compatibility
 }

 IDeviceContext* GpuContext::D3D12DeviceContext()
 {
  return DeviceContext();
 }

 DeviceResources GpuContext::D3D12DeviceResources()
 {
 DeviceResources result;
  result.pContext = pImpl_->pContext;  // implicit RefCntAutoPtr → T* conversion
  result.pDevice = pImpl_->pDevice;
  return result;
 }

 DeviceResources GpuContext::VKDeviceResources()
 {
 DeviceResources result;
  result.pContext = pImpl_->pContext;
  result.pDevice = pImpl_->pDevice;
  return result;
 }

}
