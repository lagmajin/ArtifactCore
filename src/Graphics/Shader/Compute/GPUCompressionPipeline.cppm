module;
#include <utility>

#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <cstring>
#include <memory>

module Graphics.GPUCompressionPipeline;

import Graphics.Compute;
import Graphics.Shader.Compute.HLSL.Compression;
import Graphics.GPUcomputeContext;

namespace ArtifactCore
{
  struct GPUCompressionPipeline::Impl
  {
      RefCntAutoPtr<IBuffer> pCompressionCB_;
  };

  GPUCompressionPipeline::GPUCompressionPipeline(GpuContext& context)
    : context_(context), pImpl_(new Impl())
  {
  }

  GPUCompressionPipeline::~GPUCompressionPipeline()
  {
    compressExecutor_.executor.reset();
    decompressExecutor_.executor.reset();
    delete pImpl_;
  }

  bool GPUCompressionPipeline::initialize()
  {
    if (!createConstantBuffer()) {
      return false;
    }
    if (!createExecutors()) {
      return false;
    }
    return true;
  }

  bool GPUCompressionPipeline::compress(
    IDeviceContext* ctx,
    ITextureView* srcSRV,
    IBuffer* compressedOutput,
    Uint32 width,
    Uint32 height
  ) {
    if (!ready() || !ctx || !srcSRV || !compressedOutput) {
      return false;
    }

    // Update params
    currentParams_.numBlocks = (width * height * 4 + currentParams_.blockSize - 1) / currentParams_.blockSize;
    void* pData = nullptr;
    ctx->MapBuffer(pImpl_->pCompressionCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
      memcpy(pData, &currentParams_, sizeof(CompressionParams));
      ctx->UnmapBuffer(pImpl_->pCompressionCB_, MAP_WRITE);
    }

    // Bind resources
    compressExecutor_.executor->setTextureView("g_InputTexture", srcSRV);
    compressExecutor_.executor->setBuffer("g_CompressedData", compressedOutput);

    // Dispatch
    const Uint32 numGroups = currentParams_.numBlocks;
    auto attribs = ComputeExecutor::makeDispatchAttribs(64, 1, 1);
    attribs.ThreadGroupCountX = numGroups;
    attribs.ThreadGroupCountY = 1;
    attribs.ThreadGroupCountZ = 1;

    compressExecutor_.executor->dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    return true;
  }

  bool GPUCompressionPipeline::decompress(
    IDeviceContext* ctx,
    IBuffer* compressedInput,
    ITextureView* dstUAV,
    Uint32 width,
    Uint32 height
  ) {
    if (!ready() || !ctx || !compressedInput || !dstUAV) {
      return false;
    }

    // Update params
    currentParams_.numBlocks = (width * height * 4 + currentParams_.blockSize - 1) / currentParams_.blockSize;
    void* pData = nullptr;
    ctx->MapBuffer(pImpl_->pCompressionCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
      memcpy(pData, &currentParams_, sizeof(CompressionParams));
      ctx->UnmapBuffer(pImpl_->pCompressionCB_, MAP_WRITE);
    }

    // Bind resources
    decompressExecutor_.executor->setBuffer("g_DecompressedData", compressedInput);
    decompressExecutor_.executor->setTextureView("g_OutputTexture", dstUAV);

    // Dispatch
    const Uint32 numGroups = currentParams_.numBlocks;
    auto attribs = ComputeExecutor::makeDispatchAttribs(64, 1, 1);
    attribs.ThreadGroupCountX = numGroups;
    attribs.ThreadGroupCountY = 1;
    attribs.ThreadGroupCountZ = 1;

    decompressExecutor_.executor->dispatch(ctx, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    return true;
  }

  bool GPUCompressionPipeline::ready() const
  {
    return compressExecutor_.executor && decompressExecutor_.executor && pImpl_->pCompressionCB_;
  }

  bool GPUCompressionPipeline::createConstantBuffer()
  {
    BufferDesc cbDesc;
    cbDesc.Name = "CompressionCB";
    cbDesc.Size = sizeof(CompressionParams);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    auto device = context_.RenderDevice();
    if (!device) return false;

    device->CreateBuffer(cbDesc, nullptr, &pImpl_->pCompressionCB_);
    return pImpl_->pCompressionCB_ != nullptr;
  }

  bool GPUCompressionPipeline::createExecutors()
  {
    // Compress executor
    static ShaderResourceVariableDesc compressVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_CompressedData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };

    ComputePipelineDesc compressDesc;
    compressDesc.name = "GPUCompression PSO Compress";
    compressDesc.shaderSource = Shaders::Compression::CompressSource;
    compressDesc.entryPoint = Shaders::Compression::CompressEntryPoint;
    compressDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    compressDesc.variables = compressVars;
    compressDesc.variableCount = 2;
    compressDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    compressExecutor_.executor = std::make_unique<ComputeExecutor>(context_);
    if (!compressExecutor_.executor->build(compressDesc)) {
      return false;
    }

    if (pImpl_->pCompressionCB_) {
      compressExecutor_.executor->setBuffer("CompressionCB", pImpl_->pCompressionCB_);
    }

    if (!compressExecutor_.executor->createShaderResourceBinding(true)) {
      return false;
    }

    // Decompress executor
    static ShaderResourceVariableDesc decompressVars[] = {
      {SHADER_TYPE_COMPUTE, "g_DecompressedData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };

    ComputePipelineDesc decompressDesc;
    decompressDesc.name = "GPUCompression PSO Decompress";
    decompressDesc.shaderSource = Shaders::Compression::CompressSource;
    decompressDesc.entryPoint = Shaders::Compression::DecompressEntryPoint;
    decompressDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    decompressDesc.variables = decompressVars;
    decompressDesc.variableCount = 2;
    decompressDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    decompressExecutor_.executor = std::make_unique<ComputeExecutor>(context_);
    if (!decompressExecutor_.executor->build(decompressDesc)) {
      return false;
    }

    if (pImpl_->pCompressionCB_) {
      decompressExecutor_.executor->setBuffer("CompressionCB", pImpl_->pCompressionCB_);
    }

    if (!decompressExecutor_.executor->createShaderResourceBinding(true)) {
      return false;
    }

    return true;
  }
}

