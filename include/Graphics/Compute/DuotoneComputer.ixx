module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstdint>

export module Graphics.Compute.DuotoneComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

struct DuotoneGPUParams {
    float shadowColor[4];
    float highlightColor[4];
    float blend;
};

class LIBRARY_DLL_API DuotoneGPUComputer {
public:
    explicit DuotoneGPUComputer(GpuContext &context);
    ~DuotoneGPUComputer();

    void initialize();

    void apply(IDeviceContext *pContext,
               ITextureView *inputTexture,
               ITextureView *outputTexture,
               const DuotoneGPUParams &params);

    bool ready() const;

private:
    GpuContext &context_;
    ComputeExecutor executor_;
    RefCntAutoPtr<IBuffer> pParamsCB_;

    void createPipeline();
    void createBuffers();
};

} // namespace ArtifactCore
