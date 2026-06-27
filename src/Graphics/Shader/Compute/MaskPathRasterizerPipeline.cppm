module;
#include <utility>
#include <cstring>
#include <QDebug>
#include <QPointF>
#include <QVector2D>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include "../../../../include/Define/DllExportMacro.hpp"

module Graphics.Shader.Compute.MaskPathRasterizer;

namespace ArtifactCore {

using namespace Diligent;

struct MaskPathRasterizerPipeline::Impl
{
    RefCntAutoPtr<IBuffer> pParamsCB_;
    RefCntAutoPtr<IBuffer> pSegmentBuffer_;
};

MaskPathRasterizerPipeline::MaskPathRasterizerPipeline(GpuContext& context)
    : context_(context), executor_(context), pImpl_(new Impl())
{
}

MaskPathRasterizerPipeline::~MaskPathRasterizerPipeline()
{
    delete pImpl_;
}

bool MaskPathRasterizerPipeline::initialize()
{
    if (!createConstantBuffer()) {
        return false;
    }

    static const ShaderResourceVariableDesc vars[] = {
        { SHADER_TYPE_COMPUTE, "Segments", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_COMPUTE, "OutMask",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };

    ComputePipelineDesc desc;
    desc.name = "MaskPathRasterizerPipeline";
    desc.shaderSource = maskPathRasterizerShaderText.constData();
    desc.entryPoint = "CSMain";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = static_cast<Uint32>(sizeof(vars) / sizeof(vars[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (!executor_.build(desc)) {
        qWarning() << "[MaskPathRasterizerPipeline] failed to build compute PSO";
        return false;
    }

    if (!executor_.createShaderResourceBinding(true)) {
        qWarning() << "[MaskPathRasterizerPipeline] failed to create SRB";
        return false;
    }

    if (!pImpl_->pParamsCB_) {
        return false;
    }

    executor_.setBuffer("RasterizerParams", pImpl_->pParamsCB_);
    return true;
}

bool MaskPathRasterizerPipeline::createConstantBuffer()
{
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        return false;
    }

    BufferDesc buffDesc;
    buffDesc.Name = "MaskPathRasterizerParams CB";
    buffDesc.Usage = USAGE_DYNAMIC;
    buffDesc.Size = sizeof(PathRasterizerParams);
    buffDesc.BindFlags = BIND_UNIFORM_BUFFER;
    buffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->pParamsCB_);
    if (!pImpl_->pParamsCB_) {
        qWarning() << "[MaskPathRasterizerPipeline] failed to create constant buffer";
        return false;
    }

    return true;
}

bool MaskPathRasterizerPipeline::ensureSegmentBuffer(
    const RasterizedMaskSegment* segments, uint32_t count)
{
    if (count == 0) {
        return false;
    }

    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        return false;
    }

    if (pImpl_->pSegmentBuffer_ && cachedSegmentCount_ >= count) {
        return true;
    }

    constexpr Uint32 maxSegments = 262144;
    if (count > maxSegments) {
        qWarning() << "[MaskPathRasterizerPipeline] too many segments:" << count;
        return false;
    }

    BufferDesc buffDesc;
    buffDesc.Name = "MaskPathRasterizer Segments";
    buffDesc.Usage = USAGE_DEFAULT;
    buffDesc.Size = static_cast<Uint64>(count) * sizeof(RasterizedMaskSegment);
    buffDesc.BindFlags = BIND_SHADER_RESOURCE;
    buffDesc.Mode = BUFFER_MODE_STRUCTURED;
    buffDesc.ElementByteStride = sizeof(RasterizedMaskSegment);

    BufferData initData;
    initData.pData = segments;
    initData.DataSize = buffDesc.Size;

    pImpl_->pSegmentBuffer_.Release();
    pDevice->CreateBuffer(buffDesc, &initData, &pImpl_->pSegmentBuffer_);
    if (!pImpl_->pSegmentBuffer_) {
        qWarning() << "[MaskPathRasterizerPipeline] failed to create segment buffer";
        return false;
    }

    cachedSegmentCount_ = count;
    return true;
}

bool MaskPathRasterizerPipeline::rasterizeMask(
    IDeviceContext* ctx,
    const RasterizedMaskSegment* segments,
    uint32_t numSegments,
    PathRasterizerMode mode,
    ITextureView* outUAV,
    int width, int height,
    float featherPixels)
{
    if (!ctx || !segments || numSegments == 0 || !outUAV || !ready()) {
        return false;
    }

    if (!ensureSegmentBuffer(segments, numSegments)) {
        return false;
    }

    void* pData = nullptr;
    ctx->MapBuffer(pImpl_->pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        PathRasterizerParams params;
        params.numSegments = numSegments;
        params.maskMode = static_cast<Uint32>(mode);
        params.inverted = (mode == PathRasterizerMode::AlphaInverted) ? 1 : 0;
        params.featherPixels = featherPixels;
        params.outputWidth = width;
        params.outputHeight = height;
        params.pad0 = 0;
        params.pad1 = 0;
        std::memcpy(pData, &params, sizeof(params));
        ctx->UnmapBuffer(pImpl_->pParamsCB_, MAP_WRITE);
    }

    auto* segSRV = pImpl_->pSegmentBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    if (!segSRV) {
        return false;
    }

    executor_.setBufferView("Segments", segSRV);
    executor_.setTextureView("OutMask", outUAV);

    auto attribs = ComputeExecutor::makeDispatchAttribs(
        static_cast<Uint32>(width), static_cast<Uint32>(height), 1, 8, 8, 1);
    executor_.dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return true;
}

bool MaskPathRasterizerPipeline::ready() const
{
    return executor_.ready() && pImpl_->pParamsCB_;
}

} // namespace ArtifactCore
