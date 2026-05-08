module;
#include <utility>
#include <QDebug>
#include <cstring>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include "../../../../include/Define/DllExportMacro.hpp"

module Graphics.Shader.Compute.MaskCutout;

namespace ArtifactCore {

using namespace Diligent;

struct MaskCutoutPipeline::Impl
{
    RefCntAutoPtr<IBuffer>  pMaskParamsCB_;
    RefCntAutoPtr<ITexture> pMaskTexture_;
};

MaskCutoutPipeline::MaskCutoutPipeline(GpuContext& context)
    : context_(context), executor_(context), pImpl_(new Impl())
{
}

MaskCutoutPipeline::~MaskCutoutPipeline()
{
    delete pImpl_;
}

bool MaskCutoutPipeline::initialize()
{
    if (!createConstantBuffer()) {
        return false;
    }

    static const ShaderResourceVariableDesc vars[] = {
        { SHADER_TYPE_COMPUTE, "SceneTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_COMPUTE, "MaskTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
        { SHADER_TYPE_COMPUTE, "OutTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
    };

    ComputePipelineDesc desc;
    desc.name = "MaskCutoutPipeline";
    desc.shaderSource = maskCutoutShaderText.constData();
    desc.entryPoint = "CSMain";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = static_cast<Uint32>(sizeof(vars) / sizeof(vars[0]));
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (!executor_.build(desc)) {
        qWarning() << "[MaskCutoutPipeline] failed to build compute PSO";
        return false;
    }

    if (!executor_.createShaderResourceBinding(true)) {
        qWarning() << "[MaskCutoutPipeline] failed to create SRB";
        return false;
    }

    if (!pImpl_->pMaskParamsCB_) {
        return false;
    }

    executor_.setBuffer("MaskParams", pImpl_->pMaskParamsCB_);
    return true;
}

bool MaskCutoutPipeline::createConstantBuffer()
{
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        return false;
    }

    BufferDesc buffDesc;
    buffDesc.Name = "MaskCutoutParams CB";
    buffDesc.Usage = USAGE_DYNAMIC;
    buffDesc.Size = sizeof(MaskCutoutParams);
    buffDesc.BindFlags = BIND_UNIFORM_BUFFER;
    buffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->pMaskParamsCB_);
    if (!pImpl_->pMaskParamsCB_) {
        qWarning() << "[MaskCutoutPipeline] failed to create constant buffer";
        return false;
    }

    return true;
}

bool MaskCutoutPipeline::ensureMaskTexture(const QImage& maskImage)
{
    if (maskImage.isNull()) {
        return false;
    }

    const QImage rgba = maskImage.convertToFormat(QImage::Format_RGBA8888);
    const int imgW = rgba.width();
    const int imgH = rgba.height();
    if (imgW <= 0 || imgH <= 0) {
        return false;
    }

    const qint64 cacheKey = maskImage.cacheKey();
    if (pImpl_->pMaskTexture_ && maskCacheKey_ == cacheKey && maskWidth_ == imgW && maskHeight_ == imgH) {
        return true;
    }

    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        return false;
    }

    TextureDesc texDesc;
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(imgW);
    texDesc.Height = static_cast<Uint32>(imgH);
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.SampleCount = 1;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;

    TextureSubResData subData;
    subData.pData = rgba.constBits();
    subData.Stride = static_cast<Uint64>(rgba.bytesPerLine());
    TextureData initData;
    initData.pSubResources = &subData;
    initData.NumSubresources = 1;

    pImpl_->pMaskTexture_ = nullptr;
    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pMaskTexture_);
    if (!pImpl_->pMaskTexture_) {
        qWarning() << "[MaskCutoutPipeline] failed to upload mask texture";
        return false;
    }

    maskCacheKey_ = cacheKey;
    maskWidth_ = imgW;
    maskHeight_ = imgH;
    return true;
}

bool MaskCutoutPipeline::apply(IDeviceContext* ctx,
                               const QImage& maskImage,
                               ITextureView* sceneSRV,
                               ITextureView* outUAV,
                               float opacity)
{
    if (!ctx || !sceneSRV || !outUAV || !ready()) {
        return false;
    }

    if (!ensureMaskTexture(maskImage)) {
        return false;
    }

    auto* maskSRV = pImpl_->pMaskTexture_ ? pImpl_->pMaskTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
    if (!maskSRV) {
        return false;
    }

    void* pData = nullptr;
    ctx->MapBuffer(pImpl_->pMaskParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        MaskCutoutParams params;
        params.opacity = opacity;
        std::memcpy(pData, &params, sizeof(params));
        ctx->UnmapBuffer(pImpl_->pMaskParamsCB_, MAP_WRITE);
    }

    executor_.setTextureView("SceneTex", sceneSRV);
    executor_.setTextureView("MaskTex", maskSRV);
    executor_.setTextureView("OutTex", outUAV);

    const auto* outTex = outUAV->GetTexture();
    if (!outTex) {
        return false;
    }

    const auto& desc = outTex->GetDesc();
    auto attribs = ComputeExecutor::makeDispatchAttribs(desc.Width, desc.Height, 1, 8, 8, 1);
    executor_.dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return true;
}

bool MaskCutoutPipeline::ready() const
{
    return executor_.ready() && pImpl_->pMaskParamsCB_;
}

} // namespace ArtifactCore

