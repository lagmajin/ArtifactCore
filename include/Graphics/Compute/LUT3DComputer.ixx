module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstdint>
#include <vector>

export module Graphics.Compute.LUT3DComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

/// GPU-accelerated 3D LUT application.
/// Uploads a .cube-style 3D LUT as a Texture3D and dispatches
/// a compute shader that performs manual trilinear interpolation.
class LIBRARY_DLL_API LUT3DGPUComputer {
public:
  explicit LUT3DGPUComputer(GpuContext &context);
  ~LUT3DGPUComputer();

  void initialize();

  /// Upload LUT data to a 3D texture.
  /// data must be size^3 * 3 floats (R,G,B per texel).
  void uploadLUT(IDeviceContext *pContext,
                 const float *data, int size);

  /// Apply the uploaded LUT to a texture.
  void apply(IDeviceContext *pContext,
             ITextureView *inputTexture,
             ITextureView *outputTexture);

  bool hasLUT() const;
  bool ready() const;
  int lutSize() const;

private:
  GpuContext &context_;
  ComputeExecutor executor_;
  RefCntAutoPtr<ITexture> pLUTTexture_;
  RefCntAutoPtr<ITextureView> pLUTSRV_;
  int lutSize_ = 0;

  void createPipeline();
};

} // namespace ArtifactCore
