module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>


export module Graphics.Compute.Histogram;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

HistogramComputer::HistogramComputer(GpuContext &context)
    : context_(context), executorLuminance_(context), executorRGB_(context) {}

HistogramComputer::~HistogramComputer() = default;

void HistogramComputer::initialize() {
  createPipelines();
  createBuffers();
}

void HistogramComputer::createPipelines() {
  // TODO: HLSL ソース埋め込み
  // 現在はヘッダーインターフェースのみ実装
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

bool HistogramComputer::ready() const {
  return executorLuminance_.ready() && executorRGB_.ready() &&
         pHistogramTempBuffer_;
}

} // namespace ArtifactCore
