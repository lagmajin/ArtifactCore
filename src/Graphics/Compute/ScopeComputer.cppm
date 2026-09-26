module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Compute.ScopeComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Core.ArtifactArray;
import Graphics.Shader.Compute.HLSL.ScopeVectorscope;
import Graphics.Shader.Compute.HLSL.ScopeWaveform;
import Graphics.Shader.Compute.HLSL.ScopeParade;

namespace ArtifactCore {

using namespace Diligent;

namespace {

inline constexpr const char* kScopeClearShader = R"(
cbuffer ScopeClearParams : register(b0)
{
    uint g_ElementCount;
    uint3 g_Padding;
};

RWStructuredBuffer<uint> g_Output : register(u0);

[numthreads(256, 1, 1)]
void ScopeClearCS(uint3 id : SV_DispatchThreadID)
{
    if (id.x < g_ElementCount) {
        g_Output[id.x] = 0u;
    }
}
)";

bool hasValidScopeInputs(IDeviceContext* context, ITextureView* input,
                         IBuffer* output, int step)
{
  if (!context || !input || !input->GetTexture() || !output || step <= 0) {
    return false;
  }
  const auto& inputDesc = input->GetTexture()->GetDesc();
  return inputDesc.Width > 0 && inputDesc.Height > 0;
}

bool hasBufferCapacity(const IBuffer* buffer, const uint64_t elementCount)
{
  return buffer &&
         elementCount <= std::numeric_limits<Uint64>::max() / sizeof(uint32_t) &&
         buffer->GetDesc().Size >= elementCount * sizeof(uint32_t);
}

bool hasScopeOutputCapacity(const IBuffer* buffer,
                            const uint64_t elementCount)
{
  if (!hasBufferCapacity(buffer, elementCount)) return false;
  const auto& desc = buffer->GetDesc();
  return (desc.BindFlags & BIND_UNORDERED_ACCESS) != 0 &&
         desc.Mode == BUFFER_MODE_STRUCTURED &&
         desc.ElementByteStride == sizeof(uint32_t);
}

bool calculateDispatchGroups(const ITextureView* input, const int step,
                             uint32_t& groupCount)
{
  if (!input || !input->GetTexture() || step <= 0) return false;
  const auto& desc = input->GetTexture()->GetDesc();
  const uint64_t samplesX =
      (static_cast<uint64_t>(desc.Width) + static_cast<uint32_t>(step) - 1u) /
      static_cast<uint32_t>(step);
  const uint64_t samplesY =
      (static_cast<uint64_t>(desc.Height) + static_cast<uint32_t>(step) - 1u) /
      static_cast<uint32_t>(step);
  const uint64_t totalSampled = samplesX * samplesY;
  constexpr uint64_t samplesPerGroup = 256u * 16u;
  constexpr uint64_t maxDispatchGroupsX = 65535u;
  const uint64_t groups =
      (totalSampled + samplesPerGroup - 1u) / samplesPerGroup;
  if (totalSampled == 0 ||
      totalSampled > std::numeric_limits<uint32_t>::max() ||
      groups > maxDispatchGroupsX) {
    return false;
  }
  groupCount = static_cast<uint32_t>(groups);
  return true;
}

}

ScopeComputer::ScopeComputer(GpuContext &context)
    : context_(context),
      executorVectorscope_(context),
      executorWaveform_(context),
      executorParade_(context),
      executorClear_(context) {}

ScopeComputer::~ScopeComputer() = default;

void ScopeComputer::initialize() {
  if (!context_.RenderDevice()) return;
  createPipelines();
  createBuffers();
}

void ScopeComputer::createPipelines() {
  static ShaderResourceVariableDesc scopeVars[] = {
      {SHADER_TYPE_COMPUTE, "VectorscopeParams",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputVectorscope",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  static ShaderResourceVariableDesc waveformVars[] = {
      {SHADER_TYPE_COMPUTE, "WaveformParams",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputWaveform",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  static ShaderResourceVariableDesc paradeVars[] = {
      {SHADER_TYPE_COMPUTE, "ParadeParams",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputParade",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  static ShaderResourceVariableDesc clearVars[] = {
      {SHADER_TYPE_COMPUTE, "ScopeClearParams",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Output",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  {
    ComputePipelineDesc desc;
    desc.name = "Scope/Clear";
    desc.shaderSource = kScopeClearShader;
    desc.entryPoint = "ScopeClearCS";
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = clearVars;
    desc.variableCount = 2;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executorClear_.build(desc);
    executorClear_.createShaderResourceBinding(true);
  }

  // --- Vectorscope ---
  {
    ComputePipelineDesc desc;
    desc.name = "Scope/Vectorscope";
    desc.shaderSource = Shaders::ScopeVectorscope::VectorscopeSource;
    desc.entryPoint = Shaders::ScopeVectorscope::VectorscopeEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = scopeVars;
    desc.variableCount = 3;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executorVectorscope_.build(desc);
    executorVectorscope_.createShaderResourceBinding(true);
  }

  // --- Waveform ---
  {
    ComputePipelineDesc desc;
    desc.name = "Scope/Waveform";
    desc.shaderSource = Shaders::ScopeWaveform::WaveformSource;
    desc.entryPoint = Shaders::ScopeWaveform::WaveformEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = waveformVars;
    desc.variableCount = 3;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executorWaveform_.build(desc);
    executorWaveform_.createShaderResourceBinding(true);
  }

  // --- Parade ---
  {
    ComputePipelineDesc desc;
    desc.name = "Scope/Parade";
    desc.shaderSource = Shaders::ScopeParade::ParadeSource;
    desc.entryPoint = Shaders::ScopeParade::ParadeEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = paradeVars;
    desc.variableCount = 3;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executorParade_.build(desc);
    executorParade_.createShaderResourceBinding(true);
  }
}

void ScopeComputer::createBuffers() {
  auto *device = context_.RenderDevice();
  if (!device) return;

  auto makeCB = [&](const char *name) -> RefCntAutoPtr<IBuffer> {
    BufferDesc desc;
    desc.Name = name;
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.Size = sizeof(int) * 4;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    RefCntAutoPtr<IBuffer> buf;
    device->CreateBuffer(desc, nullptr, &buf);
    return buf;
  };

  pVectorscopeParamsCB_ = makeCB("ScopeVectorscopeParams");
  pWaveformParamsCB_ = makeCB("ScopeWaveformParams");
  pParadeParamsCB_ = makeCB("ScopeParadeParams");
  pClearParamsCB_ = makeCB("ScopeClearParams");
}

bool ScopeComputer::updateParams(IDeviceContext *pContext, IBuffer *cb,
                                 int a, int b, int c, int d) {
  if (!pContext || !cb) return false;
  void *pData = nullptr;
  pContext->MapBuffer(cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
  if (!pData) return false;
  int params[4] = {a, b, c, d};
  std::memcpy(pData, params, sizeof(params));
  pContext->UnmapBuffer(cb, MAP_WRITE);
  return true;
}

bool ScopeComputer::clearOutput(IDeviceContext *pContext, IBuffer *output,
                                uint32_t elementCount) {
  if (!pContext || !output || !pClearParamsCB_ || elementCount == 0 ||
      !hasScopeOutputCapacity(output, elementCount) ||
      !updateParams(pContext, pClearParamsCB_,
                    static_cast<int>(elementCount), 0, 0, 0) ||
      !executorClear_.setBuffer("ScopeClearParams", pClearParamsCB_) ||
      !executorClear_.setBuffer("g_Output", output)) {
    return false;
  }
  executorClear_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(elementCount, 1, 1,
                                                     THREAD_GROUP_SIZE));
  const StateTransitionDesc clearBarrier{
      output,
      RESOURCE_STATE_UNORDERED_ACCESS,
      RESOURCE_STATE_UNORDERED_ACCESS};
  pContext->TransitionResourceStates(1, &clearBarrier);
  return true;
}

void ScopeComputer::computeVectorscope(IDeviceContext *pContext,
                                       ITextureView *inputTexture,
                                       IBuffer *outputVectorscope,
                                       int scopeSize, int step) {
  const uint64_t elementCount = scopeSize > 0
      ? static_cast<uint64_t>(scopeSize) * static_cast<uint64_t>(scopeSize)
      : 0;
  if (!ready() || !hasValidScopeInputs(pContext, inputTexture,
                                       outputVectorscope, step) ||
      scopeSize <= 0 || elementCount > std::numeric_limits<uint32_t>::max() ||
      !hasScopeOutputCapacity(outputVectorscope, elementCount) ||
      !clearOutput(pContext, outputVectorscope,
                   static_cast<uint32_t>(elementCount))) {
    return;
  }

  if (!updateParams(pContext, pVectorscopeParamsCB_, scopeSize, step, 0, 0) ||
      !executorVectorscope_.setBuffer("VectorscopeParams", pVectorscopeParamsCB_) ||
      !executorVectorscope_.setTextureView("g_InputTexture", inputTexture) ||
      !executorVectorscope_.setBuffer("g_OutputVectorscope", outputVectorscope)) {
    return;
  }

  uint32_t groupCount = 0;
  if (!calculateDispatchGroups(inputTexture, step, groupCount)) return;

  executorVectorscope_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     1));
}

void ScopeComputer::computeWaveform(IDeviceContext *pContext,
                                    ITextureView *inputTexture,
                                    IBuffer *outputWaveform,
                                    int outputWidth, int outputHeight,
                                    int step) {
  const uint64_t elementCount = outputWidth > 0 && outputHeight > 0
      ? static_cast<uint64_t>(outputWidth) * static_cast<uint64_t>(outputHeight)
      : 0;
  if (!ready() || !hasValidScopeInputs(pContext, inputTexture, outputWaveform,
                                       step) ||
      outputWidth <= 0 || outputHeight <= 0 ||
      elementCount > std::numeric_limits<uint32_t>::max() ||
      !hasScopeOutputCapacity(outputWaveform, elementCount) ||
      !clearOutput(pContext, outputWaveform,
                   static_cast<uint32_t>(elementCount))) {
    return;
  }

  if (!updateParams(pContext, pWaveformParamsCB_, outputWidth, outputHeight, step, 0) ||
      !executorWaveform_.setBuffer("WaveformParams", pWaveformParamsCB_) ||
      !executorWaveform_.setTextureView("g_InputTexture", inputTexture) ||
      !executorWaveform_.setBuffer("g_OutputWaveform", outputWaveform)) {
    return;
  }

  uint32_t groupCount = 0;
  if (!calculateDispatchGroups(inputTexture, step, groupCount)) return;

  executorWaveform_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     1));
}

void ScopeComputer::computeParade(IDeviceContext *pContext,
                                  ITextureView *inputTexture,
                                  IBuffer *outputParade,
                                  int outputWidth, int outputHeight,
                                  int step) {
  const uint64_t elementCount = outputWidth > 0 && outputHeight > 0
      ? static_cast<uint64_t>(outputWidth) * static_cast<uint64_t>(outputHeight) * 3u
      : 0;
  if (!ready() || !hasValidScopeInputs(pContext, inputTexture, outputParade,
                                       step) ||
      outputWidth <= 0 || outputHeight <= 0 ||
      elementCount > std::numeric_limits<uint32_t>::max() ||
      !hasScopeOutputCapacity(outputParade, elementCount) ||
      !clearOutput(pContext, outputParade,
                   static_cast<uint32_t>(elementCount))) {
    return;
  }

  if (!updateParams(pContext, pParadeParamsCB_, outputWidth, outputHeight, step, 0) ||
      !executorParade_.setBuffer("ParadeParams", pParadeParamsCB_) ||
      !executorParade_.setTextureView("g_InputTexture", inputTexture) ||
      !executorParade_.setBuffer("g_OutputParade", outputParade)) {
    return;
  }

  uint32_t groupCount = 0;
  if (!calculateDispatchGroups(inputTexture, step, groupCount)) return;

  executorParade_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     1));
}

bool ScopeComputer::enqueueReadback(IDeviceContext *pContext,
                                    IBuffer *source,
                                    IBuffer *staging,
                                    std::size_t elementCount) {
  if (!pContext || !source || !staging || elementCount == 0 ||
      elementCount > std::numeric_limits<Uint64>::max() / sizeof(uint32_t) ||
      !hasBufferCapacity(source, static_cast<uint64_t>(elementCount)) ||
      !hasBufferCapacity(staging, static_cast<uint64_t>(elementCount))) {
    return false;
  }
  const auto& stagingDesc = staging->GetDesc();
  if (stagingDesc.Usage != USAGE_STAGING ||
      (stagingDesc.CPUAccessFlags & CPU_ACCESS_READ) == 0) {
    return false;
  }

  const auto byteCount = static_cast<Uint64>(elementCount) * sizeof(uint32_t);
  pContext->CopyBuffer(source,
                       0,
                       RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                       staging,
                       0,
                       byteCount,
                       RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pContext->Flush();
  return true;
}

bool ScopeComputer::tryReadback(IDeviceContext *pContext,
                                IBuffer *staging,
                                Array<uint32_t> &dest,
                                std::size_t elementCount) {
  if (!pContext || !staging || elementCount == 0 ||
      !hasBufferCapacity(staging, static_cast<uint64_t>(elementCount)) ||
      staging->GetDesc().Usage != USAGE_STAGING ||
      (staging->GetDesc().CPUAccessFlags & CPU_ACCESS_READ) == 0) {
    return false;
  }
  void *pData = nullptr;
  pContext->MapBuffer(staging, MAP_READ, MAP_FLAG_DO_NOT_WAIT, pData);
  if (!pData) return false;

  dest.resize(elementCount);
  std::memcpy(dest.data(), pData, elementCount * sizeof(uint32_t));
  pContext->UnmapBuffer(staging, MAP_READ);

  return true;
}

bool ScopeComputer::ready() const {
  return executorVectorscope_.ready() && executorWaveform_.ready() &&
         executorParade_.ready() && executorClear_.ready() &&
         pClearParamsCB_;
}

} // namespace ArtifactCore
