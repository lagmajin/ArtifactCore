module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstdint>

export module Graphics.Compute.EdgeEchoComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

/// 3-pass GPU EdgeEcho: Sobel → Warp → Composite.
/// Manages intermediate edge and history textures internally.
class LIBRARY_DLL_API EdgeEchoGPUComputer {
public:
    explicit EdgeEchoGPUComputer(GpuContext &context);
    ~EdgeEchoGPUComputer();

    void initialize();

    /// Process one frame.
    /// @param echoColor RGBA echo color (rgb used for additive blend)
    void apply(IDeviceContext *pContext,
               ITextureView *inputTexture,
               ITextureView *outputTexture,
               float edgeThreshold,
               float decay,
               float waveFreq,
               float waveAmp,
               float timeEvolution,
               const float* echoColor);

    void reset();
    bool ready() const;

private:
    GpuContext &context_;
    ComputeExecutor executorSobel_;
    ComputeExecutor executorWarp_;
    ComputeExecutor executorComposite_;

    RefCntAutoPtr<IBuffer> pSobelParamsCB_;
    RefCntAutoPtr<IBuffer> pWarpParamsCB_;
    RefCntAutoPtr<IBuffer> pCompositeParamsCB_;

    RefCntAutoPtr<ITexture> pEdgeTexture_;       // single-channel R32_FLOAT
    RefCntAutoPtr<ITextureView> pEdgeUAV_;
    RefCntAutoPtr<ITexture> pHistoryTexture_;     // RGBA history
    RefCntAutoPtr<ITextureView> pHistorySRV_;
    RefCntAutoPtr<ITextureView> pHistoryUAV_;
    int lastWidth_ = 0, lastHeight_ = 0;
    bool hasHistory_ = false;

    void createPipelines();
    void createBuffers();
    void ensureTextures(IDeviceContext *pContext, int width, int height);
};

} // namespace ArtifactCore
