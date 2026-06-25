module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstdint>

export module Graphics.Compute.HalftoneComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

struct HalftoneGPUParams {
    float dotSize = 8.0f;
    float angleDeg = 0.0f;
    float contrast = 1.0f;
    int   colorMode = 0; // 0=mono, 1=color, 2=CMYK
    float ellipseAspect = 1.5f;
    float cmykAngles[4] = {15.0f, 75.0f, 0.0f, 45.0f};
    int   dotShape = 0; // 0=Circle, 1=Ellipse, 2=Diamond, 3=Line, 4=Cross
};

class LIBRARY_DLL_API HalftoneGPUComputer {
public:
    explicit HalftoneGPUComputer(GpuContext &context);
    ~HalftoneGPUComputer();

    void initialize();

    void apply(IDeviceContext *pContext,
               ITextureView *inputTexture,
               ITextureView *outputTexture,
               const HalftoneGPUParams &params);

    bool ready() const;

private:
    GpuContext &context_;
    ComputeExecutor executor_;
    RefCntAutoPtr<IBuffer> pParamsCB_;

    void createPipeline();
    void createBuffers();
};

} // namespace ArtifactCore
