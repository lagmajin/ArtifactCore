module;
#include <algorithm>
#include <cstddef>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <cstdint>
#include <cstring>
#include <limits>


module Graphics.Compute.Histogram;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.Histogram;

namespace ArtifactCore {

using namespace Diligent;

namespace {

inline constexpr const char* kHistogramClearShader = R"(
cbuffer HistogramClearParams : register(b0)
{
    uint g_ElementCount;
    uint g_StatisticsLayout;
    uint2 g_Padding;
};

RWStructuredBuffer<uint> g_Output : register(u0);

[numthreads(256, 1, 1)]
void HistogramClearCS(uint3 id : SV_DispatchThreadID)
{
    if (id.x < g_ElementCount) {
        g_Output[id.x] = (g_StatisticsLayout != 0u && id.x == 0u)
            ? 0xffffffffu : 0u;
    }
}
)";

bool hasHistogramOutput(const IBuffer* output, const uint64_t elementCount)
{
  if (!output ||
      elementCount > std::numeric_limits<Uint64>::max() / sizeof(uint32_t)) {
    return false;
  }
  const auto& outputDesc = output->GetDesc();
  return outputDesc.Size >= elementCount * sizeof(uint32_t) &&
         (outputDesc.BindFlags & BIND_UNORDERED_ACCESS) != 0 &&
         outputDesc.Mode == BUFFER_MODE_STRUCTURED &&
         outputDesc.ElementByteStride == sizeof(uint32_t);
}

bool hasHistogramInputs(IDeviceContext* context, ITextureView* input,
                        IBuffer* output, const uint64_t elementCount)
{
  if (!context || !input || !input->GetTexture() ||
      !hasHistogramOutput(output, elementCount)) {
    return false;
  }
  const auto& inputDesc = input->GetTexture()->GetDesc();
  return inputDesc.Width > 0 && inputDesc.Height > 0;
}

bool updateRegionParams(IDeviceContext* context, IBuffer* buffer, uint32_t x,
                        uint32_t y, uint32_t width, uint32_t height)
{
  if (!context || !buffer) {
    return false;
  }
  const uint32_t params[4] = {x, y, width, height};
  void* data = nullptr;
  context->MapBuffer(buffer, MAP_WRITE, MAP_FLAG_DISCARD, data);
  if (!data) {
    return false;
  }
  std::memcpy(data, params, sizeof(params));
  context->UnmapBuffer(buffer, MAP_WRITE);
  return true;
}

}

HistogramComputer::HistogramComputer(GpuContext &context)
    : context_(context), executorLuminance_(context), executorRGB_(context),
      executorStatistics_(context), executorClear_(context) {}

HistogramComputer::~HistogramComputer() = default;

void HistogramComputer::initialize() {
  if (!context_.RenderDevice()) {
    return;
  }
  createPipelines();
  createBuffers();
}

void HistogramComputer::createPipelines() {
  static ShaderResourceVariableDesc lumaVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputHistogram", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "HistogramParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  static ShaderResourceVariableDesc rgbVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputHistogramRGB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "HistogramParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  static ShaderResourceVariableDesc statisticsVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputStatistics", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "HistogramParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  static ShaderResourceVariableDesc clearVars[] = {
      {SHADER_TYPE_COMPUTE, "HistogramClearParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Output", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  ComputePipelineDesc lumaDesc;
  lumaDesc.name = "Histogram/Luma";
  lumaDesc.shaderSource = Shaders::Histogram::HistogramSource;
  lumaDesc.entryPoint = Shaders::Histogram::HistogramLumaEntryPoint;
  lumaDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  lumaDesc.variables = lumaVars;
  lumaDesc.variableCount = 3;
  lumaDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

  ComputePipelineDesc rgbDesc = lumaDesc;
  rgbDesc.name = "Histogram/RGB";
  rgbDesc.entryPoint = Shaders::Histogram::HistogramRGBEntryPoint;
  rgbDesc.variables = rgbVars;

  ComputePipelineDesc statsDesc = lumaDesc;
  statsDesc.name = "Histogram/Statistics";
  statsDesc.entryPoint = Shaders::Histogram::StatisticsEntryPoint;
  statsDesc.variables = statisticsVars;

  ComputePipelineDesc clearDesc;
  clearDesc.name = "Histogram/Clear";
  clearDesc.shaderSource = kHistogramClearShader;
  clearDesc.entryPoint = "HistogramClearCS";
  clearDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  clearDesc.variables = clearVars;
  clearDesc.variableCount = 2;
  clearDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

  executorLuminance_.build(lumaDesc);
  executorRGB_.build(rgbDesc);
  executorStatistics_.build(statsDesc);
  executorClear_.build(clearDesc);

  executorLuminance_.createShaderResourceBinding(true);
  executorRGB_.createShaderResourceBinding(true);
  executorStatistics_.createShaderResourceBinding(true);
  executorClear_.createShaderResourceBinding(true);
}

void HistogramComputer::createBuffers() {
  auto* device = context_.RenderDevice();
  if (!device) {
    return;
  }
  BufferDesc statsDesc;
  statsDesc.Name = "HistogramStatisticsParams";
  statsDesc.Usage = USAGE_DYNAMIC;
  statsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  statsDesc.Size = sizeof(uint32_t) * 4;
  statsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  device->CreateBuffer(statsDesc, nullptr, &pStatisticsParamsBuffer_);

  BufferDesc clearDesc = statsDesc;
  clearDesc.Name = "HistogramClearParams";
  device->CreateBuffer(clearDesc, nullptr, &pClearParamsBuffer_);
}

bool HistogramComputer::clearOutput(IDeviceContext *pContext, IBuffer *output,
                                    uint32_t elementCount,
                                    bool statisticsLayout) {
  if (!pContext || !pClearParamsBuffer_ || elementCount == 0 ||
      !hasHistogramOutput(output, elementCount)) {
    return false;
  }
  const uint32_t params[4] = {
      elementCount, statisticsLayout ? 1u : 0u, 0u, 0u};
  void *data = nullptr;
  pContext->MapBuffer(pClearParamsBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, data);
  if (!data) return false;
  std::memcpy(data, params, sizeof(params));
  pContext->UnmapBuffer(pClearParamsBuffer_, MAP_WRITE);
  if (!executorClear_.setBuffer("HistogramClearParams", pClearParamsBuffer_) ||
      !executorClear_.setBuffer("g_Output", output)) {
    return false;
  }
  executorClear_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(
                    elementCount, 1, 1, THREAD_GROUP_SIZE));
  const StateTransitionDesc barrier{output, RESOURCE_STATE_UNORDERED_ACCESS,
                                    RESOURCE_STATE_UNORDERED_ACCESS};
  pContext->TransitionResourceStates(1, &barrier);
  return true;
}

void HistogramComputer::computeLuminance(IDeviceContext *pContext,
                                         ITextureView *inputTexture,
                                         IBuffer *outputHistogram) {
  computeLuminanceRegion(pContext, inputTexture, 0, 0, 0, 0,
                         outputHistogram);
}

void HistogramComputer::computeLuminanceRegion(
    IDeviceContext *pContext, ITextureView *inputTexture, uint32_t x,
    uint32_t y, uint32_t regionWidth, uint32_t regionHeight,
    IBuffer *outputHistogram) {
  if (!ready() || !hasHistogramInputs(pContext, inputTexture, outputHistogram,
                                      BinCount) ||
      !clearOutput(pContext, outputHistogram, BinCount)) {
    return;
  }
  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;
  const bool regionEnabled = regionWidth != 0 && regionHeight != 0;
  if (regionEnabled && (x >= width || y >= height)) {
    return;
  }
  const uint32_t safeRegionWidth =
      regionEnabled ? std::min(regionWidth, width - x) : 0u;
  const uint32_t safeRegionHeight =
      regionEnabled ? std::min(regionHeight, height - y) : 0u;
  if (!updateRegionParams(pContext, pStatisticsParamsBuffer_,
                          regionEnabled ? x : 0u,
                          regionEnabled ? y : 0u,
                          safeRegionWidth, safeRegionHeight) ||
      !executorLuminance_.setTextureView("g_InputTexture", inputTexture) ||
      !executorLuminance_.setBuffer("g_OutputHistogram", outputHistogram) ||
      !executorLuminance_.setBuffer("HistogramParams",
                                    pStatisticsParamsBuffer_)) {
    return;
  }

  const uint64_t threadCount =
      static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
  constexpr uint64_t samplesPerGroup = THREAD_GROUP_SIZE * 16ull;
  const uint64_t groupCount64 =
      (threadCount + samplesPerGroup - 1) / samplesPerGroup;
  if (threadCount > std::numeric_limits<uint32_t>::max() ||
      groupCount64 == 0 || groupCount64 > 65535u) {
    return;
  }
  const uint32_t groupCount = static_cast<uint32_t>(groupCount64);

  executorLuminance_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     1));
}

void HistogramComputer::computeRGB(IDeviceContext *pContext,
                                   ITextureView *inputTexture,
                                   IBuffer *outputHistogram) {
  if (!ready() || !hasHistogramInputs(pContext, inputTexture, outputHistogram,
                                      BinCount * 3ull) ||
      !updateRegionParams(pContext, pStatisticsParamsBuffer_, 0, 0, 0, 0) ||
      !clearOutput(pContext, outputHistogram, BinCount * 3u) ||
      !executorRGB_.setTextureView("g_InputTexture", inputTexture) ||
      !executorRGB_.setBuffer("g_OutputHistogramRGB", outputHistogram) ||
      !executorRGB_.setBuffer("HistogramParams", pStatisticsParamsBuffer_)) {
    return;
  }

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;

  const uint64_t threadCount =
      static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
  constexpr uint64_t samplesPerGroup = THREAD_GROUP_SIZE * 8ull;
  const uint64_t groupCount64 =
      (threadCount + samplesPerGroup - 1) / samplesPerGroup;
  if (threadCount > std::numeric_limits<uint32_t>::max() ||
      groupCount64 == 0 || groupCount64 > 65535u) {
    return;
  }
  const uint32_t groupCount = static_cast<uint32_t>(groupCount64);

  executorRGB_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(
                                      groupCount, 1, 1, 1));
}

void HistogramComputer::computeStatistics(IDeviceContext *pContext,
                                          ITextureView *inputTexture,
                                          uint32_t x, uint32_t y,
                                          uint32_t width, uint32_t height,
                                          IBuffer *outputStatistics) {
  if (!ready() ||
      !hasHistogramInputs(pContext, inputTexture, outputStatistics,
                          StatisticsWordCount) ||
      !pStatisticsParamsBuffer_ ||
      !clearOutput(pContext, outputStatistics, StatisticsWordCount, true)) {
    return;
  }

  const auto texWidth = inputTexture->GetTexture()->GetDesc().Width;
  const auto texHeight = inputTexture->GetTexture()->GetDesc().Height;
  const bool regionEnabled = width != 0 && height != 0;
  if (regionEnabled && (x >= texWidth || y >= texHeight)) {
    return;
  }
  const uint32_t dispatchWidth =
      regionEnabled ? std::min(width, texWidth - x) : texWidth;
  const uint32_t dispatchHeight =
      regionEnabled ? std::min(height, texHeight - y) : texHeight;
  const uint32_t regionX = regionEnabled ? x : 0u;
  const uint32_t regionY = regionEnabled ? y : 0u;
  const uint32_t regionWidth = regionEnabled ? dispatchWidth : 0u;
  const uint32_t regionHeight = regionEnabled ? dispatchHeight : 0u;
  constexpr uint32_t maxDispatchExtent = 65535u * 16u;
  if (dispatchWidth == 0 || dispatchHeight == 0 ||
      dispatchWidth > maxDispatchExtent ||
      dispatchHeight > maxDispatchExtent) {
    return;
  }
  if (!updateRegionParams(pContext, pStatisticsParamsBuffer_, regionX, regionY,
                          regionWidth, regionHeight)) {
    return;
  }

  if (!executorStatistics_.setTextureView("g_InputTexture", inputTexture) ||
      !executorStatistics_.setBuffer("HistogramParams",
                                     pStatisticsParamsBuffer_) ||
      !executorStatistics_.setBuffer("g_OutputStatistics", outputStatistics)) {
    return;
  }

  auto attribs =
      ComputeExecutor::makeDispatchAttribs(dispatchWidth, dispatchHeight, 1, 16, 16, 1);
  executorStatistics_.dispatch(pContext, attribs);
}

ImageHistogramStatistics HistogramComputer::decodeStatistics(
    const uint32_t *words, std::size_t wordCount) {
  ImageHistogramStatistics result;
  if (!words || wordCount < StatisticsWordCount) return result;
  result.sampleCount = words[2];
  result.minBin = result.sampleCount == 0 ? 0u : words[0];
  result.maxBin = words[1];
  result.sumBins = static_cast<uint64_t>(words[3]) |
                   (static_cast<uint64_t>(words[4]) << 32u);
  result.sumSquaredBins = static_cast<uint64_t>(words[5]) |
                          (static_cast<uint64_t>(words[6]) << 32u);
  result.highClippedPixels = words[7];
  result.lowClippedPixels = words[8];
  result.channelClippedPixels = words[9];
  return result;
}

bool HistogramComputer::ready() const {
  return executorLuminance_.ready() && executorRGB_.ready() &&
         executorStatistics_.ready() && executorClear_.ready() &&
         pStatisticsParamsBuffer_ && pClearParamsBuffer_;
}

} // namespace ArtifactCore
