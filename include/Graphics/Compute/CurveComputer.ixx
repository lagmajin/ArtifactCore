module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <array>
#include <cstdint>

export module Graphics.Compute.CurveComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

/// GPU-accelerated per-pixel color curve application.
/// Expects 4 LUTs of 256 floats each (master, R, G, B).
class LIBRARY_DLL_API CurveGPUComputer {
public:
  explicit CurveGPUComputer(GpuContext &context);
  ~CurveGPUComputer();

  void initialize();

  /// Apply curves to a texture.
  /// @param inputTexture  SRV (RGBA8_UNORM)
  /// @param outputTexture UAV (same format)
  /// @param luts  flattened float[1024]: [master(256), red(256), green(256), blue(256)]
  /// @param masterOnly  apply only master curve to all channels
  void apply(IDeviceContext *pContext,
             ITextureView *inputTexture,
             ITextureView *outputTexture,
             const float *luts,
             bool masterOnly);

  bool ready() const;

private:
  GpuContext &context_;
  ComputeExecutor executor_;
  RefCntAutoPtr<IBuffer> pParamsCB_;
  RefCntAutoPtr<IBuffer> pLUTBuffer_;

  void createPipeline();
  void createBuffers();
};

} // namespace ArtifactCore
