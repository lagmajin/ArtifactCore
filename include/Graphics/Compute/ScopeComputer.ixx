module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstddef>
#include <cstdint>

export module Graphics.Compute.ScopeComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
export import Core.ArtifactArray;

export namespace ArtifactCore {

using namespace Diligent;

/// GPU-accelerated vectorscope / waveform / parade measurement.
/// Follows the same pattern as HistogramComputer:
///   compute*() dispatches a compute shader that accumulates into a
///   caller-provided RWStructuredBuffer<uint>.
/// Readback staging is caller-owned so a fixed ring can be reused without
/// allocating or waiting in the render hot path.
class LIBRARY_DLL_API ScopeComputer {
public:
  explicit ScopeComputer(GpuContext &context);
  ~ScopeComputer();

  void initialize();

  /// --- Vectorscope ---
  /// Output buffer: uint32_t[scopeSize * scopeSize]
  /// It must be a structured UAV with a 4-byte element stride.
  /// Each bin counts pixels whose Cb/Cr falls at that scope coordinate.
  void computeVectorscope(IDeviceContext *pContext,
                          ITextureView *inputTexture,
                          IBuffer *outputVectorscope,
                          int scopeSize = 256, int step = 2);

  /// --- Waveform (luminance) ---
  /// Output buffer: uint32_t[outputWidth * outputHeight]
  /// It must be a structured UAV with a 4-byte element stride.
  /// X = source column remapped, Y = luminance (0 = black)
  void computeWaveform(IDeviceContext *pContext,
                       ITextureView *inputTexture,
                       IBuffer *outputWaveform,
                       int outputWidth = 256, int outputHeight = 128,
                       int step = 2);

  /// --- Parade (R / G / B per-column) ---
  /// Output buffer: uint32_t[outputWidth * outputHeight * 3]
  /// It must be a structured UAV with a 4-byte element stride.
  /// Panes: [0..pane-1] = R, [pane..2*pane-1] = G, [2*pane..3*pane-1] = B
  void computeParade(IDeviceContext *pContext,
                     ITextureView *inputTexture,
                     IBuffer *outputParade,
                     int outputWidth = 256, int outputHeight = 128,
                     int step = 2);

  /// Enqueues an asynchronous copy into a caller-owned staging buffer.
  /// The staging buffer must be USAGE_STAGING with CPU_ACCESS_READ and have
  /// capacity for elementCount uint32 values. Callers should rotate a bounded
  /// staging ring rather than reuse a buffer whose copy is still pending.
  static bool enqueueReadback(IDeviceContext *pContext,
                              IBuffer *source,
                              IBuffer *staging,
                              std::size_t elementCount);

  /// Polls a previously enqueued staging buffer without blocking the GPU.
  /// Reserve dest once during setup to keep successful polls allocation-free.
  static bool tryReadback(IDeviceContext *pContext,
                          IBuffer *staging,
                          Array<uint32_t> &dest,
                          std::size_t elementCount);

  bool ready() const;

private:
  GpuContext &context_;
  ComputeExecutor executorVectorscope_;
  ComputeExecutor executorWaveform_;
  ComputeExecutor executorParade_;
  ComputeExecutor executorClear_;

  void createPipelines();
  void createBuffers();
  bool updateParams(IDeviceContext *pContext, IBuffer *cb,
                    int a, int b, int c, int d);
  bool clearOutput(IDeviceContext *pContext, IBuffer *output,
                   uint32_t elementCount);

  RefCntAutoPtr<IBuffer> pVectorscopeParamsCB_;
  RefCntAutoPtr<IBuffer> pWaveformParamsCB_;
  RefCntAutoPtr<IBuffer> pParadeParamsCB_;
  RefCntAutoPtr<IBuffer> pClearParamsCB_;

  static constexpr uint32_t THREAD_GROUP_SIZE = 256;
};

} // namespace ArtifactCore
