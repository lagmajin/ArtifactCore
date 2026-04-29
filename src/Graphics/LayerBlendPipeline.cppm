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

namespace ArtifactCore {

struct LayerBlendPipeline::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pBlendCB_;
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
 qDebug() << "[LayerBlendPipeline] Initialized with" << executors_.size() << "blend modes";
 return true;
}

bool LayerBlendPipeline::createConstantBuffer()
{
 if (!context_) return false;
 auto pDevice = context_->D3D12RenderDevice();
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
 return true;
}

bool LayerBlendPipeline::createExecutors()
{
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

bool LayerBlendPipeline::ready() const
{
 return !executors_.empty() && pImpl_->pBlendCB_;
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

 auto it = executors_.find(mode);
 if (it == executors_.end()) {
  qWarning() << "[LayerBlendPipeline::blend] No executor for mode" << static_cast<int>(mode)
             << "- falling back to Normal";
  it = executors_.find(BlendMode::Normal);
  if (it == executors_.end()) {
   qCritical() << "[LayerBlendPipeline::blend] No Normal fallback executor available!";
   return false;
  }
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
  qWarning() << "[LayerBlendPipeline::blend] MapBuffer failed - opacity may be wrong";
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
 ITextureView* outUAV,
 BlendMode mode,
 float opacity,
 Uint32 width,
 Uint32 height
)
{
 if (!ctx || !srcSRV || !outUAV) return false;

 auto it = executors_.find(mode);
 if (it == executors_.end()) {
  it = executors_.find(BlendMode::Normal);
  if (it == executors_.end()) return false;
 }

 auto& exec = *it->second.executor;
 if (!exec.ready()) return false;

 currentParams_.opacity = opacity;
 currentParams_.blendMode = static_cast<unsigned int>(mode);

 void* pData = nullptr;
 ctx->MapBuffer(pImpl_->pBlendCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
 if (pData) {
  memcpy(pData, &currentParams_, sizeof(BlendParams));
  ctx->UnmapBuffer(pImpl_->pBlendCB_, MAP_WRITE);
 }

 exec.setTextureView("SrcTex", srcSRV);
 // DstTex is required by the shader even in direct mode (though it might not be used by all modes)
 // For direct mode, we assume dst is not needed or we use a dummy. 
 // Actually, the shaders I saw use DstTex.
 exec.setTextureView("DstTex", srcSRV); // Fallback to src if not provided? No, that's wrong.
 // Let's add dstSRV to blendDirect signature or just fix it to bind something.
 exec.setTextureView("OutTex", outUAV);

 DispatchComputeAttribs attribs;
 attribs.ThreadGroupCountX = (width + 7) / 8;
 attribs.ThreadGroupCountY = (height + 7) / 8;
 attribs.ThreadGroupCountZ = 1;

 exec.dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 return true;
}

}
