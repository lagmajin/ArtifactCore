module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QDebug>

module Graphics.LayerBlendPipeline;

namespace {
inline constexpr const char* kMatteTrackSource = R"(
cbuffer MatteTrackParams : register(b0)
{
    uint  g_MatteCount;
    uint  g_MatteMode0;
    uint  g_MatteMode1;
    uint  g_MatteMode2;
    uint  g_StackMode;
    uint  g_LumaMode;
    float g_Opacity;
    float _pad0;
};

Texture2D<float4>  g_LayerTex  : register(t0);
Texture2D<float4>  g_MatteSrc0 : register(t1);
Texture2D<float4>  g_MatteSrc1 : register(t2);
Texture2D<float4>  g_MatteSrc2 : register(t3);
RWTexture2D<float4> g_OutTex    : register(u0);

static const float3 kLumaRec601 = float3(0.299f, 0.587f, 0.114f);
static const float3 kLumaRec709 = float3(0.2126f, 0.7152f, 0.0722f);

float extractMask(float4 color, uint mode, float3 lumaCoeffs)
{
    float mask;
    if (mode == 0 || mode == 2) {
        mask = color.a;
    } else {
        mask = dot(color.rgb, lumaCoeffs);
    }
    if (mode == 2 || mode == 3) {
        mask = 1.0f - mask;
    }
    return saturate(mask);
}

float combineMasks(float a, float b, uint mode)
{
    if (mode == 0) return saturate(a + b);
    if (mode == 1) return min(a, b);
    return saturate(a - b);
}

[numthreads(16, 16, 1)]
void MatteTrackCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutTex.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float3 lumaCoeffs = (g_LumaMode == 1) ? kLumaRec709 : kLumaRec601;
    float combinedMask = 1.0f;

    [branch] if (g_MatteCount > 0) {
        float4 matteColor = g_MatteSrc0.Load(int3(id.xy, 0));
        combinedMask = extractMask(matteColor, g_MatteMode0, lumaCoeffs);
    }

    [branch] if (g_MatteCount > 1) {
        float4 matteColor = g_MatteSrc1.Load(int3(id.xy, 0));
        float mask = extractMask(matteColor, g_MatteMode1, lumaCoeffs);
        combinedMask = combineMasks(combinedMask, mask, g_StackMode);
    }

    [branch] if (g_MatteCount > 2) {
        float4 matteColor = g_MatteSrc2.Load(int3(id.xy, 0));
        float mask = extractMask(matteColor, g_MatteMode2, lumaCoeffs);
        combinedMask = combineMasks(combinedMask, mask, g_StackMode);
    }

    combinedMask *= g_Opacity;

    float4 layerColor = g_LayerTex.Load(int3(id.xy, 0));
    g_OutTex[id.xy] = float4(layerColor.rgb * combinedMask, layerColor.a * combinedMask);
}
)";
inline constexpr const char* kMatteTrackEntryPoint = "MatteTrackCS";
}

namespace ArtifactCore {

struct LayerBlendPipeline::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pBlendCB_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pMatteTrackCB_;
};

LayerBlendPipeline::LayerBlendPipeline(std::shared_ptr<GpuContext> context)
    : context_(std::move(context)), pImpl_(new Impl())
{
}

LayerBlendPipeline::~LayerBlendPipeline()
{
    delete pImpl_;
}

bool LayerBlendPipeline::initialize()
{
 if (!createConstantBuffer()) return false;
 if (!createExecutors()) return false;
 const bool matteTrackReady = createMatteTrackExecutor();
 if (!matteTrackReady) {
  qWarning() << "[LayerBlendPipeline] standard blend ready without track matte capability";
 }
 qDebug() << "[LayerBlendPipeline] Initialized with" << executors_.size()
          << "blend modes; matteTrackReady=" << matteTrackReady;
 return ready();
}

bool LayerBlendPipeline::createConstantBuffer()
{
 if (!context_) return false;
 auto pDevice = context_->RenderDevice();
 if (!pDevice) return false;

 BufferDesc buffDesc;
 buffDesc.Name           = "BlendParams CB";
 buffDesc.Usage          = USAGE_DYNAMIC;
 buffDesc.Size           = sizeof(BlendParams);
 buffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
 // [Fix 1] USAGE_DYNAMIC には CPU_ACCESS_WRITE が必須
 buffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

  pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->pBlendCB_);
  if (!pImpl_->pBlendCB_) {
   qWarning() << "[LayerBlendPipeline] Failed to create constant buffer";
   return false;
  }

  BufferDesc mtDesc;
  mtDesc.Name           = "MatteTrackParams CB";
  mtDesc.Usage          = USAGE_DYNAMIC;
  mtDesc.Size           = sizeof(MatteTrackParams);
  mtDesc.BindFlags      = BIND_UNIFORM_BUFFER;
  mtDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  pDevice->CreateBuffer(mtDesc, nullptr, &pImpl_->pMatteTrackCB_);
  if (!pImpl_->pMatteTrackCB_) {
   qWarning() << "[LayerBlendPipeline] Failed to create optional matte track constant buffer";
  }

  return true;
}

bool LayerBlendPipeline::createExecutors()
{
 static ShaderResourceVariableDesc layerToFloatVars[] = {
  {SHADER_TYPE_COMPUTE, "SrcTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  {SHADER_TYPE_COMPUTE, "OutTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
 };
 layerToFloatExecutor_ = std::make_unique<ComputeExecutor>(*context_);
 ComputePipelineDesc layerToFloatDesc;
 layerToFloatDesc.name = "LayerToFloat PSO";
 layerToFloatDesc.shaderSource = layerToFloatShaderText.constData();
 layerToFloatDesc.entryPoint = "main";
 layerToFloatDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
 layerToFloatDesc.variables = layerToFloatVars;
 layerToFloatDesc.variableCount = 2;
 layerToFloatDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
 if (!layerToFloatExecutor_->build(layerToFloatDesc) ||
     !layerToFloatExecutor_->createShaderResourceBinding(true)) {
  qWarning() << "[LayerBlendPipeline] layer-to-float conversion PSO build failed";
  layerToFloatExecutor_.reset();
 }

 static ShaderResourceVariableDesc channelComponentDisplayVars[] = {
  {SHADER_TYPE_COMPUTE, "SrcTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  {SHADER_TYPE_COMPUTE, "OutTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
 };
 channelComponentDisplayExecutor_ = std::make_unique<ComputeExecutor>(*context_);
 ComputePipelineDesc channelComponentDisplayDesc;
 channelComponentDisplayDesc.name = "Channel Component Display PSO";
 channelComponentDisplayDesc.shaderSource =
     channelComponentDisplayShaderText.constData();
 channelComponentDisplayDesc.entryPoint = "main";
 channelComponentDisplayDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
 channelComponentDisplayDesc.variables = channelComponentDisplayVars;
 channelComponentDisplayDesc.variableCount = 2;
 channelComponentDisplayDesc.defaultVariableType =
     SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
 if (!channelComponentDisplayExecutor_->build(channelComponentDisplayDesc) ||
     !channelComponentDisplayExecutor_->setBuffer("BlendParams", pImpl_->pBlendCB_) ||
     !channelComponentDisplayExecutor_->createShaderResourceBinding(true)) {
  qWarning() << "[LayerBlendPipeline] channel component display PSO build failed";
  channelComponentDisplayExecutor_.reset();
 }

 // [Fix A] Vars[] の変数名はシェーダに合わせて "OutTex" を宣言
 static ShaderResourceVariableDesc Vars[] = {
  {SHADER_TYPE_COMPUTE, "SrcTex",      SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  {SHADER_TYPE_COMPUTE, "DstTex",      SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  {SHADER_TYPE_COMPUTE, "OutTex",      SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
 };

 int successCount = 0;
 int failCount    = 0;

 for (const auto& [mode, shaderCode] : BlendShaders) {
  BlendExecutor entry;
  entry.executor = std::make_unique<ComputeExecutor>(*context_);

  ComputePipelineDesc desc;
  desc.name               = "Blend PSO";
  desc.shaderSource       = shaderCode.constData();
  desc.entryPoint         = "main";
  desc.sourceLanguage     = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables          = Vars;
  desc.variableCount      = 3;
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

  if (!entry.executor->build(desc)) {
   qWarning() << "[LayerBlendPipeline] PSO build FAILED for blend mode" << static_cast<int>(mode);
   ++failCount;
   continue;
  }

  if (pImpl_->pBlendCB_) {
   entry.executor->setBuffer("BlendParams", pImpl_->pBlendCB_);
  } else {
   qWarning() << "[LayerBlendPipeline] pBlendCB_ is null for blend mode" << static_cast<int>(mode)
              << "- opacity will not work";
  }

  if (!entry.executor->createShaderResourceBinding(true)) {
   qWarning() << "[LayerBlendPipeline] SRB creation FAILED for blend mode" << static_cast<int>(mode);
   ++failCount;
   continue;
  }

  executors_.emplace(mode, std::move(entry));
  ++successCount;
 }

 qDebug() << "[LayerBlendPipeline] createExecutors done:"
          << successCount << "succeeded," << failCount << "failed"
          << "out of" << static_cast<int>(BlendShaders.size()) << "modes";
 return !executors_.empty();
}

bool LayerBlendPipeline::createMatteTrackExecutor()
{
    if (!pImpl_->pMatteTrackCB_) {
        return false;
    }
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "g_LayerTex",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_MatteSrc0", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_MatteSrc1", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_MatteSrc2", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutTex",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    matteTrackExecutor_ = std::make_unique<ComputeExecutor>(*context_);
    ComputePipelineDesc desc;
    desc.name = "MatteTrack PSO";
    desc.shaderSource = kMatteTrackSource;
    desc.entryPoint = kMatteTrackEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 5;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!matteTrackExecutor_->build(desc)) {
        qWarning() << "[LayerBlendPipeline] MatteTrack PSO build failed";
        matteTrackExecutor_.reset();
        return false;
    }
    if (pImpl_->pMatteTrackCB_) {
        matteTrackExecutor_->setBuffer("MatteTrackParams", pImpl_->pMatteTrackCB_);
    }
    if (!matteTrackExecutor_->createShaderResourceBinding(true)) {
        qWarning() << "[LayerBlendPipeline] MatteTrack SRB creation failed";
        matteTrackExecutor_.reset();
        return false;
    }
    return true;
}

bool LayerBlendPipeline::applyTrackMatte(
    IDeviceContext* ctx,
    ITextureView* layerSRV,
    ITextureView* matteSrc0SRV,
    ITextureView* matteSrc1SRV,
    ITextureView* matteSrc2SRV,
    ITextureView* outUAV,
    const MatteTrackParams& params,
    Uint32 width,
    Uint32 height)
{
    if (!ctx || !layerSRV || !matteSrc0SRV || !outUAV) {
        qWarning() << "[LayerBlendPipeline::applyTrackMatte] invalid input";
        return false;
    }
    if (!matteTrackExecutor_ || !matteTrackExecutor_->ready()) {
        qWarning() << "[LayerBlendPipeline::applyTrackMatte] executor not ready";
        return false;
    }
    if (!pImpl_->pMatteTrackCB_) {
        qWarning() << "[LayerBlendPipeline::applyTrackMatte] constant buffer not initialized";
        return false;
    }

    // Write constant buffer
    void* pData = nullptr;
    ctx->MapBuffer(pImpl_->pMatteTrackCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        memcpy(pData, &params, sizeof(MatteTrackParams));
        ctx->UnmapBuffer(pImpl_->pMatteTrackCB_, MAP_WRITE);
    }

    matteTrackExecutor_->setTextureView("g_LayerTex", layerSRV);
    matteTrackExecutor_->setTextureView("g_MatteSrc0", matteSrc0SRV);
    matteTrackExecutor_->setTextureView("g_MatteSrc1", matteSrc1SRV);
    matteTrackExecutor_->setTextureView("g_MatteSrc2", matteSrc2SRV);
    matteTrackExecutor_->setTextureView("g_OutTex", outUAV);

    DispatchComputeAttribs attribs;
    attribs.ThreadGroupCountX = (width + 15) / 16;
    attribs.ThreadGroupCountY = (height + 15) / 16;
    attribs.ThreadGroupCountZ = 1;
    matteTrackExecutor_->dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return true;
}

bool LayerBlendPipeline::ready() const
{
 return !executors_.empty() && pImpl_->pBlendCB_ && layerToFloatExecutor_ &&
        layerToFloatExecutor_->ready();
}

bool LayerBlendPipeline::convertLayerToFloat(
 IDeviceContext* ctx,
 ITextureView* srcSRV,
 ITextureView* outUAV,
 Uint32 width,
 Uint32 height
)
{
 if (!ctx || !srcSRV || !outUAV || !layerToFloatExecutor_ ||
     !layerToFloatExecutor_->ready() || width == 0 || height == 0) {
  qCritical() << "[LayerBlendPipeline::convertLayerToFloat] invalid input"
              << "ctx=" << static_cast<bool>(ctx)
              << "srcSRV=" << static_cast<bool>(srcSRV)
              << "outUAV=" << static_cast<bool>(outUAV)
              << "executor=" << static_cast<bool>(layerToFloatExecutor_)
              << "width=" << width
              << "height=" << height;
  return false;
 }

 const auto* srcTexture = srcSRV->GetTexture();
 const auto* outTexture = outUAV->GetTexture();
 if (!srcTexture || !outTexture || srcTexture == outTexture) {
  qCritical() << "[LayerBlendPipeline::convertLayerToFloat] invalid resource alias";
  return false;
 }

 const auto& srcDesc = srcTexture->GetDesc();
 const auto& outDesc = outTexture->GetDesc();
 if (srcDesc.Width != width || srcDesc.Height != height ||
     outDesc.Width != width || outDesc.Height != height ||
     (outDesc.Format != TEX_FORMAT_RGBA32_FLOAT &&
      outDesc.Format != TEX_FORMAT_RGBA16_FLOAT)) {
  qCritical() << "[LayerBlendPipeline::convertLayerToFloat] texture contract mismatch"
              << "requested=" << width << "x" << height
              << "src=" << srcDesc.Width << "x" << srcDesc.Height
              << "srcFormat=" << static_cast<int>(srcDesc.Format)
              << "out=" << outDesc.Width << "x" << outDesc.Height
              << "outFormat=" << static_cast<int>(outDesc.Format);
  return false;
 }

 layerToFloatExecutor_->setTextureView("SrcTex", srcSRV);
 layerToFloatExecutor_->setTextureView("OutTex", outUAV);

 auto attribs = ComputeExecutor::makeDispatchAttribs(width, height, 1, 8, 8, 1);
 layerToFloatExecutor_->dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}

bool LayerBlendPipeline::displayComponent(
 IDeviceContext* ctx,
 ITextureView* srcSRV,
 ITextureView* outUAV,
 Uint32 component,
 Uint32 width,
 Uint32 height
)
{
 if (!ctx || !srcSRV || !outUAV || !channelComponentDisplayExecutor_ ||
     !channelComponentDisplayExecutor_->ready() || !pImpl_->pBlendCB_ ||
     component > 3 || width == 0 || height == 0) {
  return false;
 }

 const auto* srcTexture = srcSRV->GetTexture();
 const auto* outTexture = outUAV->GetTexture();
 if (!srcTexture || !outTexture || srcTexture == outTexture) {
  return false;
 }

 const auto& srcDesc = srcTexture->GetDesc();
 const auto& outDesc = outTexture->GetDesc();
 const bool formatsMatch =
     (srcDesc.Format == TEX_FORMAT_RGBA32_FLOAT ||
      srcDesc.Format == TEX_FORMAT_RGBA16_FLOAT) &&
     outDesc.Format == srcDesc.Format;
 if (srcDesc.Width != width || srcDesc.Height != height ||
     outDesc.Width != width || outDesc.Height != height || !formatsMatch) {
  return false;
 }

 BlendParams displayParams;
 displayParams.blendMode = component;
 void* pData = nullptr;
 ctx->MapBuffer(pImpl_->pBlendCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
 if (!pData) {
  return false;
 }
 memcpy(pData, &displayParams, sizeof(displayParams));
 ctx->UnmapBuffer(pImpl_->pBlendCB_, MAP_WRITE);

 if (!channelComponentDisplayExecutor_->setTextureView("SrcTex", srcSRV) ||
     !channelComponentDisplayExecutor_->setTextureView("OutTex", outUAV)) {
  return false;
 }

 const auto attribs =
     ComputeExecutor::makeDispatchAttribs(width, height, 1, 8, 8, 1);
 channelComponentDisplayExecutor_->dispatch(
     ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}

bool LayerBlendPipeline::blend(
 IDeviceContext* ctx,
 ITextureView* srcSRV,
 ITextureView* dstSRV,
 ITextureView* outUAV,
 BlendMode mode,
 float opacity
)
{
 // [Fix: 詳細チェック]
 if (!ctx) {
  qCritical() << "[LayerBlendPipeline::blend] ctx is null";
  return false;
 }
 if (!srcSRV) {
  qCritical() << "[LayerBlendPipeline::blend] srcSRV is null for mode" << static_cast<int>(mode);
  return false;
 }
 if (!dstSRV) {
  qCritical() << "[LayerBlendPipeline::blend] dstSRV is null for mode" << static_cast<int>(mode);
  return false;
 }
 if (!outUAV) {
  qCritical() << "[LayerBlendPipeline::blend] outUAV is null for mode" << static_cast<int>(mode);
  return false;
 }
 if (!pImpl_->pBlendCB_) {
  qCritical() << "[LayerBlendPipeline::blend] pBlendCB_ is null - constant buffer not initialized";
  return false;
 }

 const auto* srcTexture = srcSRV->GetTexture();
 const auto* dstTexture = dstSRV->GetTexture();
 const auto* outTexture = outUAV->GetTexture();
 if (!srcTexture || !dstTexture || !outTexture ||
     srcTexture == outTexture || dstTexture == outTexture) {
  qCritical() << "[LayerBlendPipeline::blend] invalid resource contract";
  return false;
 }

 const auto& srcDesc = srcTexture->GetDesc();
 const auto& dstDesc = dstTexture->GetDesc();
 const auto& outDesc = outTexture->GetDesc();
 const bool dimensionsMatch =
     srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height &&
     srcDesc.Width == outDesc.Width && srcDesc.Height == outDesc.Height;
 const bool formatsMatch =
     (srcDesc.Format == TEX_FORMAT_RGBA32_FLOAT ||
      srcDesc.Format == TEX_FORMAT_RGBA16_FLOAT) &&
     dstDesc.Format == srcDesc.Format &&
     outDesc.Format == srcDesc.Format;
 if (!dimensionsMatch || !formatsMatch) {
  qCritical() << "[LayerBlendPipeline::blend] canonical texture contract mismatch"
              << "srcFormat=" << static_cast<int>(srcDesc.Format)
              << "dstFormat=" << static_cast<int>(dstDesc.Format)
              << "outFormat=" << static_cast<int>(outDesc.Format);
  return false;
 }

 auto it = executors_.find(mode);
 if (it == executors_.end()) {
  qCritical() << "[LayerBlendPipeline::blend] No executor for requested mode"
              << static_cast<int>(mode);
  return false;
 }

 auto& exec = *it->second.executor;
 if (!exec.ready()) {
  qCritical() << "[LayerBlendPipeline::blend] executor not ready for mode" << static_cast<int>(mode);
  return false;
 }

 currentParams_.opacity   = opacity;
 currentParams_.blendMode = static_cast<unsigned int>(mode);

 void* pData = nullptr;
 ctx->MapBuffer(pImpl_->pBlendCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
 if (pData) {
  memcpy(pData, &currentParams_, sizeof(BlendParams));
  ctx->UnmapBuffer(pImpl_->pBlendCB_, MAP_WRITE);
 } else {
  qCritical() << "[LayerBlendPipeline::blend] MapBuffer failed";
  return false;
 }

 exec.setTextureView("SrcTex", srcSRV);
 exec.setTextureView("DstTex", dstSRV);
 exec.setTextureView("OutTex", outUAV);

 auto attribs = ComputeExecutor::makeDispatchAttribs(64, 8, 1);
 const auto& texDesc = outUAV->GetTexture()->GetDesc();
 attribs.ThreadGroupCountX = (texDesc.Width  + 7) / 8;
 attribs.ThreadGroupCountY = (texDesc.Height + 7) / 8;
 attribs.ThreadGroupCountZ = 1;

 exec.dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}

bool LayerBlendPipeline::blendDirect(
 IDeviceContext* ctx,
 ITextureView* srcSRV,
 ITextureView* dstSRV,
 ITextureView* outUAV,
 BlendMode mode,
 float opacity,
 Uint32 width,
 Uint32 height
)
{
 if (!ctx || !srcSRV || !dstSRV || !outUAV) {
  qCritical() << "[LayerBlendPipeline::blendDirect] invalid input"
              << "ctx=" << static_cast<bool>(ctx)
              << "srcSRV=" << static_cast<bool>(srcSRV)
              << "dstSRV=" << static_cast<bool>(dstSRV)
              << "outUAV=" << static_cast<bool>(outUAV);
  return false;
 }

 auto it = executors_.find(mode);
 if (it == executors_.end()) {
  it = executors_.find(BlendMode::Normal);
  if (it == executors_.end()) {
   return false;
  }
 }

 auto& exec = *it->second.executor;
 if (!exec.ready()) {
  return false;
 }

 currentParams_.opacity = opacity;
 currentParams_.blendMode = static_cast<unsigned int>(mode);

 void* pData = nullptr;
 ctx->MapBuffer(pImpl_->pBlendCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
 if (pData) {
  memcpy(pData, &currentParams_, sizeof(BlendParams));
  ctx->UnmapBuffer(pImpl_->pBlendCB_, MAP_WRITE);
 }

 exec.setTextureView("SrcTex", srcSRV);
 exec.setTextureView("DstTex", dstSRV);
 exec.setTextureView("OutTex", outUAV);

 DispatchComputeAttribs attribs;
 attribs.ThreadGroupCountX = (width + 7) / 8;
 attribs.ThreadGroupCountY = (height + 7) / 8;
 attribs.ThreadGroupCountZ = 1;

 exec.dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}

bool LayerBlendPipeline::blendDirect(
 IDeviceContext* ctx,
 ITextureView* srcSRV,
 ITextureView* outUAV,
 BlendMode mode,
 float opacity,
 Uint32 width,
 Uint32 height
)
{
 qWarning() << "[LayerBlendPipeline::blendDirect] legacy overload called without dstSRV";
 return false;
}

}
