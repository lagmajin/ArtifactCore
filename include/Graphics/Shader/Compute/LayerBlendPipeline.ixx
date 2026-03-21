module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../Define/DllExportMacro.hpp"
#include <map>
#include <memory>

export module Graphics.LayerBlendPipeline;

import Graphics.Compute;
import Graphics.Shader.Compute.HLSL.Blend;
import Graphics.GPUcomputeContext;
import Layer.Blend;

export namespace ArtifactCore
{
 using namespace Diligent;

 struct BlendParams {
  float opacity = 1.0f;
  unsigned int blendMode = 0;
  float _pad0 = 0.0f;
  float _pad1 = 0.0f;
 };

 class LIBRARY_DLL_API LayerBlendPipeline
 {
 public:
  explicit LayerBlendPipeline(GpuContext& context);
  ~LayerBlendPipeline();

  bool initialize();

  bool blend(
   IDeviceContext* ctx,
   ITextureView* srcSRV,
   ITextureView* dstSRV,
   ITextureView* outUAV,
   BlendMode mode,
   float opacity
  );

  bool blendDirect(
   IDeviceContext* ctx,
   ITextureView* srcSRV,
   ITextureView* outUAV,
   BlendMode mode,
   float opacity,
   Uint32 width,
   Uint32 height
  );

  bool ready() const;

 private:
  bool createConstantBuffer();
  bool createExecutors();

  struct BlendExecutor {
   std::unique_ptr<ComputeExecutor> executor;
  };

  GpuContext& context_;
  RefCntAutoPtr<IBuffer> pBlendCB_;
  std::map<BlendMode, BlendExecutor> executors_;
  BlendParams currentParams_{};
 };

}
