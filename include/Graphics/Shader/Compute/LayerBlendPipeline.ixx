module;
#include <utility>
// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround)
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

  struct MatteTrackParams {
   unsigned int matteCount = 1;
   unsigned int matteMode0 = 0;   // 0=Alpha, 1=Luma, 2=AlphaInv, 3=LumaInv
   unsigned int matteMode1 = 0;
   unsigned int matteMode2 = 0;
   unsigned int stackMode = 0;    // 0=Add, 1=Common, 2=Subtract
   unsigned int lumaMode = 0;     // 0=Rec.601, 1=Rec.709
   float opacity = 1.0f;
   float _pad0 = 0.0f;
  };

 class LIBRARY_DLL_API LayerBlendPipeline
 {
 public:
  explicit LayerBlendPipeline(std::shared_ptr<GpuContext> context);
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

  bool convertLayerToFloat(
   IDeviceContext* ctx,
   ITextureView* srcSRV,
   ITextureView* outUAV,
   Uint32 width,
   Uint32 height
  );

  bool blendDirect(
   IDeviceContext* ctx,
   ITextureView* srcSRV,
   ITextureView* dstSRV,
   ITextureView* outUAV,
   BlendMode mode,
   float opacity,
   Uint32 width,
   Uint32 height
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

  bool applyTrackMatte(
   IDeviceContext* ctx,
   ITextureView* layerSRV,
   ITextureView* matteSrc0SRV,
   ITextureView* matteSrc1SRV,
   ITextureView* matteSrc2SRV,
   ITextureView* outUAV,
   const MatteTrackParams& params,
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

  bool createMatteTrackExecutor();

  std::shared_ptr<GpuContext> context_;
  class Impl;
  Impl* pImpl_ = nullptr;
  std::unique_ptr<ComputeExecutor> layerToFloatExecutor_;
  std::map<BlendMode, BlendExecutor> executors_;
  BlendParams currentParams_{};
  std::unique_ptr<ComputeExecutor> matteTrackExecutor_;
 };

}
