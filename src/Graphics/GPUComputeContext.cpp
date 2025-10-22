module;

#include <vulkan/vulkan.h>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>

#include <QSysInfo>
#include <QOperatingSystemVersion>
#include <QUuid>
module Graphics:GPUcomputeContext;


namespace Diligent {}//dummy


namespace ArtifactCore
{
 using namespace Diligent;

 struct GpuContext::Impl
 {
  QUuid	dx12id;
  RefCntAutoPtr<IRenderDevice> pD3D12Device;
  RefCntAutoPtr<IDeviceContext> pD3D12Context;

  RefCntAutoPtr<IRenderDevice> pVKRenderDevice;
  RefCntAutoPtr<IDeviceContext> pVKContext;
 	
  RefCntAutoPtr<IRenderDevice> pMetalDevice;
  RefCntAutoPtr<IDeviceContext> pMetalContext;

 	
  GPUInfo info() const
  {
   QString osType = QSysInfo::productType(); // "windows", "osx", "linux" など

   auto result=GPUInfo();

   auto adapter = GraphicsAdapterInfo();
  	
   if (osType == "windows")
   {
    if (pD3D12Device)
    {
     adapter = pD3D12Device->GetAdapterInfo();
    
    }
    else if (pVKRenderDevice)
    {
      adapter = pVKRenderDevice->GetAdapterInfo();
     // info.name = adapter.Description;
    }
   }
   else if (osType == "osx")
   {
    if (pMetalDevice)
    {
     adapter = pMetalDevice->GetAdapterInfo();
     // info.name = adapter.Description;
    }
   }
   else if (osType == "linux")
   {
    if (pVKRenderDevice)
    {
     adapter = pVKRenderDevice->GetAdapterInfo();
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
  Diligent::ShaderCreateInfo shaderCI;
  shaderCI.Desc.ShaderType = type;
  shaderCI.Desc.Name = "MyShader";
  shaderCI.EntryPoint = entryPoint;
  shaderCI.Source = shaderSource;
  shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  //shaderCI.UseCombinedTextureSamplers = true;

  Diligent::RefCntAutoPtr<Diligent::IShader> shader;
  pD3D12Device->CreateShader(shaderCI, &shader);
  return shader;
 }

 void GpuContext::Impl::Initialize()
 {
  EngineD3D12CreateInfo engineD3D12CI;



  auto pD3D12Factory = GetEngineFactoryD3D12();

  pD3D12Factory->CreateDeviceAndContextsD3D12(engineD3D12CI, &pD3D12Device, &pD3D12Context);



  //pD3D12Device->GetDeviceInfo().Features.

  //EngineVkCreateInfo engineVKCI;

  //auto pVKFactory = GetEngineFactoryVk();

  //pVKFactory->CreateDeviceAndContextsVk(engineVKCI, &pVKRenderDevice, &pVKContext);
 }

 GpuContext::GpuContext() :pImpl_(new Impl())
 {

 }

 GpuContext::~GpuContext()
 {
  delete pImpl_;
 }


 RefCntAutoPtr<IShader> GpuContext::CompileShader(const char* shaderSource, Diligent::SHADER_TYPE type, const char* entryPoint)
 {
  Diligent::RefCntAutoPtr<IShader> pShader;
  pShader = nullptr;

  return pShader;
 }

 void GpuContext::Initialize()
 {
  pImpl_->Initialize();
 }

 RefCntAutoPtr<IRenderDevice> GpuContext::D3D12RenderDevice()
 {
  return pImpl_->pD3D12Device;
 }

 Diligent::RefCntAutoPtr<IDeviceContext> GpuContext::D3D12DeviceContext()
 {
  return pImpl_->pD3D12Context;
 }

 DeviceResources GpuContext::D3D12DeviceResources()
 {
  DeviceResources result;
  result.pContext = pImpl_->pD3D12Context;
  result.pDevice = pImpl_->pD3D12Device;

  return result;
 }

 DeviceResources GpuContext::VKDeviceResources()
 {
  DeviceResources result;
  //result.pContext = pImpl_->pD3D12Context;
  //result.pDevice = pImpl_->pD3D12Device;

  return result;
 }

}
