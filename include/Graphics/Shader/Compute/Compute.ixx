module;
#include <utility>
// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround)
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
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
  ~ComputeExecutor();

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
  class Impl;
  Impl* pImpl_ = nullptr;
 };
}
