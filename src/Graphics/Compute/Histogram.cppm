module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>


module Graphics.Compute.Histogram;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.Histogram;

namespace ArtifactCore {

using namespace Diligent;

HistogramComputer::HistogramComputer(GpuContext &context)
    : context_(context), executorLuminance_(context), executorRGB_(context),
      executorStatistics_(context) {}

HistogramComputer::~HistogramComputer() = default;

void HistogramComputer::initialize() {
  createPipelines();
  createBuffers();
}

void HistogramComputer::createPipelines() {
  static ShaderResourceVariableDesc histogramVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputHistogram", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputHistogramRGB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputStatistics", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  ComputePipelineDesc lumaDesc;
  lumaDesc.name = "Histogram/Luma";
  lumaDesc.shaderSource = Shaders::Histogram::HistogramSource;
  lumaDesc.entryPoint = Shaders::Histogram::HistogramLumaEntryPoint;
  lumaDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  lumaDesc.variables = histogramVars;
  lumaDesc.variableCount = 4;
  lumaDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

  ComputePipelineDesc rgbDesc = lumaDesc;
  rgbDesc.name = "Histogram/RGB";
  rgbDesc.entryPoint = Shaders::Histogram::HistogramRGBEntryPoint;

  ComputePipelineDesc statsDesc = lumaDesc;
  statsDesc.name = "Histogram/Statistics";
  statsDesc.entryPoint = Shaders::Histogram::StatisticsEntryPoint;

  executorLuminance_.build(lumaDesc);
  executorRGB_.build(rgbDesc);
  executorStatistics_.build(statsDesc);

  executorLuminance_.createShaderResourceBinding(true);
  executorRGB_.createShaderResourceBinding(true);
  executorStatistics_.createShaderResourceBinding(true);
}

void HistogramComputer::createBuffers() {
  BufferDesc buffDesc;
  buffDesc.Name = "HistogramTempBuffer";
  buffDesc.Usage = USAGE_DEFAULT;
  buffDesc.BindFlags = BIND_UNORDERED_ACCESS;
  buffDesc.Size = sizeof(uint32_t) * BIN_COUNT;
  buffDesc.Mode = BUFFER_MODE_STRUCTURED;
  buffDesc.ElementByteStride = sizeof(uint32_t);

  context_.device()->CreateBuffer(buffDesc, nullptr, &pHistogramTempBuffer_);

  BufferDesc statsDesc;
  statsDesc.Name = "HistogramStatisticsParams";
  statsDesc.Usage = USAGE_DYNAMIC;
  statsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  statsDesc.Size = sizeof(uint32_t) * 5;
  statsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  context_.device()->CreateBuffer(statsDesc, nullptr, &pStatisticsParamsBuffer_);
}

void HistogramComputer::computeLuminance(IDeviceContext *pContext,
                                         ITextureView *inputTexture,
                                         IBuffer *outputHistogram) {
  if (!ready())
    return;

  executorLuminance_.setTextureView("g_InputTexture", inputTexture);
  executorLuminance_.setBuffer("g_OutputHistogram", outputHistogram);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;

  const uint32_t threadCount = width * height;
  const uint32_t groupCount =
      (threadCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;

  executorLuminance_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     THREAD_GROUP_SIZE));
}

void HistogramComputer::computeLuminanceRegion(IDeviceContext *pContext,
                                               ITextureView *inputTexture,
                                               uint32_t x, uint32_t y,
                                               uint32_t width, uint32_t height,
                                               IBuffer *outputHistogram) {
  // 領域指定版は後で実装
  computeLuminance(pContext, inputTexture, outputHistogram);
}

void HistogramComputer::computeRGB(IDeviceContext *pContext,
                                   ITextureView *inputTexture,
                                   IBuffer *outputHistogram) {
  if (!ready())
    return;

  executorRGB_.setTextureView("g_InputTexture", inputTexture);
  executorRGB_.setBuffer("g_OutputHistogram", outputHistogram);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;

  const uint32_t threadCount = width * height;
  const uint32_t groupCount =
      (threadCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;

  executorRGB_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(
                                      groupCount, 1, 1, THREAD_GROUP_SIZE));
}

void HistogramComputer::computeStatistics(IDeviceContext *pContext,
                                          ITextureView *inputTexture,
                                          uint32_t x, uint32_t y,
                                          uint32_t width, uint32_t height,
                                          IBuffer *outputStatistics) {
  if (!ready() || !inputTexture || !outputStatistics ||
      !pStatisticsParamsBuffer_) {
    return;
  }

  uint32_t params[4] = {x, y, width, height};
  void *pData = nullptr;
  pContext->MapBuffer(pStatisticsParamsBuffer_, MAP_WRITE, MAP_FLAG_DISCARD,
                      pData);
  if (pData) {
    auto *dst = static_cast<uint32_t *>(pData);
    for (int i = 0; i < 4; ++i) {
      dst[i] = params[i];
    }
    pContext->UnmapBuffer(pStatisticsParamsBuffer_, MAP_WRITE);
  }

  uint32_t init[5] = {0xFFFFFFFFu, 0u, 0u, 0u, 0u};
  void *outData = nullptr;
  pContext->MapBuffer(outputStatistics, MAP_WRITE, MAP_FLAG_DISCARD, outData);
  if (outData) {
    auto *dst = static_cast<uint32_t *>(outData);
    for (int i = 0; i < 5; ++i) {
      dst[i] = init[i];
    }
    pContext->UnmapBuffer(outputStatistics, MAP_WRITE);
  }

  executorStatistics_.setTextureView("g_InputTexture", inputTexture);
  executorStatistics_.setBuffer("HistogramParams", pStatisticsParamsBuffer_);
  executorStatistics_.setBuffer("g_OutputStatistics", outputStatistics);

  const auto texWidth = inputTexture->GetTexture()->GetDesc().Width;
  const auto texHeight = inputTexture->GetTexture()->GetDesc().Height;
  const uint32_t dispatchWidth = width == 0 ? texWidth : width;
  const uint32_t dispatchHeight = height == 0 ? texHeight : height;

  auto attribs =
      ComputeExecutor::makeDispatchAttribs(dispatchWidth, dispatchHeight, 1, 16, 16, 1);
  executorStatistics_.dispatch(pContext, attribs);
}

bool HistogramComputer::ready() const {
  return executorLuminance_.ready() && executorRGB_.ready() &&
         executorStatistics_.ready() && pHistogramTempBuffer_ &&
         pStatisticsParamsBuffer_;
}

} // namespace ArtifactCore
