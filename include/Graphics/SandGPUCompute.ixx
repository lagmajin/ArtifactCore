module;
#include <vector>
#include <cstdint>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include "../Define/DllExportMacro.hpp"

export module Graphics.SandGPUCompute;

import Graphics.GPUcomputeContext;
import Graphics.Compute;

export namespace ArtifactCore {

using namespace Diligent;

class LIBRARY_DLL_API SandGPUCompute {
public:
    explicit SandGPUCompute(GpuContext& context);
    ~SandGPUCompute();

    bool initialize(int width, int height);
    void simulate(IDeviceContext* pContext, int substeps = 1);
    void renderToOutput(IDeviceContext* pContext, ITextureView* outputUAV);

    void uploadFromCPU(IDeviceContext* pContext, const std::vector<uint8_t>& grid, int width, int height);
    void readbackToCPU(IDeviceContext* pContext, std::vector<uint8_t>& grid);

    int width() const { return width_; }
    int height() const { return height_; }
    bool ready() const { return ready_; }

    ITextureView* gridSRV() const;

private:
    bool createTextures();
    bool createConstantBuffer();
    bool buildPipelines();

    GpuContext& context_;
    ComputeExecutor simulateExec_;
    ComputeExecutor renderExec_;

    int width_ = 0;
    int height_ = 0;
    bool ready_ = false;
    int pingPong_ = 0;

    struct SimParams {
        Uint32 width = 0;
        Uint32 height = 0;
        Uint32 substep = 0;
        Uint32 padding = 0;
    };
    SimParams params_;

    RefCntAutoPtr<IBuffer> pConstantBuffer_;
    RefCntAutoPtr<ITexture> pGridTex_[2];
    RefCntAutoPtr<ITexture> pOutputTex_;
};

} // namespace ArtifactCore
