module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <array>
#include <cstdint>
#include <vector>

export module Graphics.Compute.EchoBlendComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

/// GPU-accelerated echo blend kernel.
/// CPU manages the ring buffer; GPU accumulates weighted frames.
/// Up to ECHO_MAX_RING (=8) ring entries can be blended.
class LIBRARY_DLL_API EchoBlendGPUComputer {
public:
    explicit EchoBlendGPUComputer(GpuContext &context);
    ~EchoBlendGPUComputer();

    void initialize();

    /// Blend ring buffer entries into output.
    /// @param ringTextures SRVs for each ring entry (0=current frame).
    ///   Must have exactly frameCount textures (1..8).
    void apply(IDeviceContext *pContext,
               ITextureView *inputTexture,
               ITextureView *outputTexture,
               const std::vector<ITextureView*> &ringTextures,
               int frameCount,
               float decay,
               float startingIntensity);

    bool ready() const;

private:
    GpuContext &context_;
    ComputeExecutor executor_;
    RefCntAutoPtr<IBuffer> pParamsCB_;
    std::array<RefCntAutoPtr<ITexture>, 8> pDummyTextures_;

    void createPipeline();
    void createBuffers();
    void bindRingEntries(IDeviceContext *pContext,
                         const std::vector<ITextureView*> &ring);
};

} // namespace ArtifactCore
