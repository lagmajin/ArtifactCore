module;
#include <utility>
#include <memory>
#include <vector>
#include <QImage>
#include <QByteArray>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../Define/DllExportMacro.hpp"

export module Graphics.Shader.Compute.MaskPathRasterizer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.MaskPathRasterizer;

export namespace ArtifactCore {
using namespace Diligent;

enum class PathRasterizerMode {
    Alpha,
    AlphaInverted
};

struct PathRasterizerParams {
    Uint32 numSegments;
    Uint32 maskMode;
    Uint32 inverted;
    float  featherPixels;
    Int32  outputWidth;
    Int32  outputHeight;
    Uint32 pad0;
    Uint32 pad1;
};

struct RasterizedMaskSegment {
    float startX, startY;
    float endX, endY;
};

class LIBRARY_DLL_API MaskPathRasterizerPipeline {
public:
    explicit MaskPathRasterizerPipeline(GpuContext& context);
    ~MaskPathRasterizerPipeline();

    bool initialize();

    bool rasterizeMask(
        IDeviceContext* ctx,
        const RasterizedMaskSegment* segments,
        uint32_t numSegments,
        PathRasterizerMode mode,
        ITextureView* outUAV,
        int width, int height,
        float featherPixels = 0.0f);

    bool ready() const;

private:
    bool createConstantBuffer();
    bool ensureSegmentBuffer(const RasterizedMaskSegment* segments, uint32_t count);

    GpuContext& context_;
    ComputeExecutor executor_;
    class Impl;
    Impl* pImpl_ = nullptr;
    uint32_t cachedSegmentCount_ = 0;
};

} // namespace ArtifactCore
