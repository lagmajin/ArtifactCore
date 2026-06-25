module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstdint>
#include <vector>

export module Graphics.Compute.ScopeComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

/// GPU-accelerated vectorscope / waveform / parade measurement.
/// Follows the same pattern as HistogramComputer:
///   compute*() dispatches a compute shader that accumulates into a
///   caller-provided RWStructuredBuffer<uint>.
///   readbackResults() copies the GPU buffer to a CPU-side uint32 vector.
class LIBRARY_DLL_API ScopeComputer {
public:
  explicit ScopeComputer(GpuContext &context);
  ~ScopeComputer();

  void initialize();

  /// --- Vectorscope ---
  /// Output buffer: uint32_t[scopeSize * scopeSize]
  /// Each bin counts pixels whose Cb/Cr falls at that scope coordinate.
  void computeVectorscope(IDeviceContext *pContext,
                          ITextureView *inputTexture,
                          IBuffer *outputVectorscope,
                          int scopeSize = 256, int step = 2);

  /// --- Waveform (luminance) ---
  /// Output buffer: uint32_t[outputWidth * outputHeight]
  /// X = source column remapped, Y = luminance (0 = black)
  void computeWaveform(IDeviceContext *pContext,
                       ITextureView *inputTexture,
                       IBuffer *outputWaveform,
                       int outputWidth = 256, int outputHeight = 128,
                       int step = 2);

  /// --- Parade (R / G / B per-column) ---
  /// Output buffer: uint32_t[outputWidth * outputHeight * 3]
  /// Panes: [0..pane-1] = R, [pane..2*pane-1] = G, [2*pane..3*pane-1] = B
  void computeParade(IDeviceContext *pContext,
                     ITextureView *inputTexture,
                     IBuffer *outputParade,
                     int outputWidth = 256, int outputHeight = 128,
                     int step = 2);

  /// --- Readback helper ---
  /// Copies a RWStructuredBuffer<uint> back to a CPU-side vector.
  /// Creates and destroys a staging buffer internally.
  static bool readbackResults(IDeviceContext *pContext,
                              IRenderDevice *pDevice,
                              IBuffer *source,
                              std::vector<uint32_t> &dest,
                              size_t elementCount);

  bool ready() const;

private:
  GpuContext &context_;
  ComputeExecutor executorVectorscope_;
  ComputeExecutor executorWaveform_;
  ComputeExecutor executorParade_;

  void createPipelines();
  void createBuffers();
  void updateParams(IDeviceContext *pContext, IBuffer *cb,
                    int a, int b, int c, int d);

  RefCntAutoPtr<IBuffer> pVectorscopeParamsCB_;
  RefCntAutoPtr<IBuffer> pWaveformParamsCB_;
  RefCntAutoPtr<IBuffer> pParadeParamsCB_;

  static constexpr uint32_t THREAD_GROUP_SIZE = 256;
};

} // namespace ArtifactCore
