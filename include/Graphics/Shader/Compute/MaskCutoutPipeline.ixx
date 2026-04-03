module;
#include <QImage>
#include <QByteArray>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../Define/DllExportMacro.hpp"

export module Graphics.Shader.Compute.MaskCutout;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.MaskCutout;

export namespace ArtifactCore {

using namespace Diligent;

struct MaskCutoutParams {
    float opacity = 1.0f;
    float pad0 = 0.0f;
    float pad1 = 0.0f;
    float pad2 = 0.0f;
};

class LIBRARY_DLL_API MaskCutoutPipeline {
public:
    explicit MaskCutoutPipeline(GpuContext& context);
    ~MaskCutoutPipeline();

    bool initialize();
    bool apply(IDeviceContext* ctx,
               const QImage& maskImage,
               ITextureView* sceneSRV,
               ITextureView* outUAV,
               float opacity = 1.0f);

    bool ready() const;

private:
    bool createConstantBuffer();
    bool ensureMaskTexture(const QImage& maskImage);

    GpuContext& context_;
    ComputeExecutor executor_;
    RefCntAutoPtr<IBuffer> pMaskParamsCB_;
    RefCntAutoPtr<ITexture> pMaskTexture_;
    qint64 maskCacheKey_ = -1;
    int maskWidth_ = 0;
    int maskHeight_ = 0;
};

} // namespace ArtifactCore
