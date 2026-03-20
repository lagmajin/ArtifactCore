module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../Define/DllExportMacro.hpp"

export module Graphics.Compute;

import Graphics.GPUcomputeContext;

export namespace ArtifactCore
{
 using namespace Diligent;

 struct ComputePipelineDesc
 {
  const char* name = "ComputePipeline";
  const char* shaderSource = nullptr;
  const char* entryPoint = "CSMain";
  SHADER_SOURCE_LANGUAGE sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  const ShaderResourceVariableDesc* variables = nullptr;
  Uint32 variableCount = 0;
  SHADER_RESOURCE_VARIABLE_TYPE defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
 };

 class LIBRARY_DLL_API ComputeExecutor
 {
 public:
  explicit ComputeExecutor(GpuContext& context);

  bool build(const ComputePipelineDesc& desc);
  bool createShaderResourceBinding(bool initializeStaticResources = true);

  bool setBuffer(const char* name, IBuffer* buffer);
  bool setBufferView(const char* name, IBufferView* view);
  bool setTextureView(const char* name, ITextureView* view);
  bool setSampler(const char* name, ISampler* sampler);

  void dispatch(IDeviceContext* pContext,
   const DispatchComputeAttribs& attribs,
   RESOURCE_STATE_TRANSITION_MODE transitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  static DispatchComputeAttribs makeDispatchAttribs(
   Uint32 width,
   Uint32 height = 1,
   Uint32 depth = 1,
   Uint32 threadsX = 64,
   Uint32 threadsY = 1,
   Uint32 threadsZ = 1);

  bool ready() const;
  IPipelineState* pipelineState() const;
  IShaderResourceBinding* shaderResourceBinding() const;

 private:
  template <typename ResourceT>
  bool setResource(const char* name, ResourceT* resource);

  GpuContext& context_;
  RefCntAutoPtr<IShader> pComputeShader_;
  RefCntAutoPtr<IPipelineState> pPSO_;
  RefCntAutoPtr<IShaderResourceBinding> pSRB_;
 };

 template <typename ResourceT>
 bool ComputeExecutor::setResource(const char* name, ResourceT* resource)
 {
  if (name == nullptr || resource == nullptr || pPSO_ == nullptr)
   return false;

  if (auto* staticVar = pPSO_->GetStaticVariableByName(SHADER_TYPE_COMPUTE, name))
  {
   staticVar->Set(resource);
   return true;
  }

  if (pSRB_ != nullptr)
  {
   if (auto* var = pSRB_->GetVariableByName(SHADER_TYPE_COMPUTE, name))
   {
    var->Set(resource);
    return true;
   }
  }

  return false;
 }
}
