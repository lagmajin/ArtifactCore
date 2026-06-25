module;
#include <cstdint>
#include <cstring>
#include <vector>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Graphics.Compute.ScopeComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.ScopeVectorscope;
import Graphics.Shader.Compute.HLSL.ScopeWaveform;
import Graphics.Shader.Compute.HLSL.ScopeParade;

export namespace ArtifactCore {

using namespace Diligent;

ScopeComputer::ScopeComputer(GpuContext &context)
    : context_(context),
      executorVectorscope_(context),
      executorWaveform_(context),
      executorParade_(context) {}

ScopeComputer::~ScopeComputer() = default;

void ScopeComputer::initialize() {
  createPipelines();
  createBuffers();
}

void ScopeComputer::createPipelines() {
  static ShaderResourceVariableDesc scopeVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputVectorscope",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  static ShaderResourceVariableDesc waveformVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputWaveform",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  static ShaderResourceVariableDesc paradeVars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputParade",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  // --- Vectorscope ---
  {
    ComputePipelineDesc desc;
    desc.name = "Scope/Vectorscope";
    desc.shaderSource = Shaders::ScopeVectorscope::VectorscopeSource;
    desc.entryPoint = Shaders::ScopeVectorscope::VectorscopeEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = scopeVars;
    desc.variableCount = 2;
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
    desc.variableCount = 2;
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
    desc.variableCount = 2;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executorParade_.build(desc);
    executorParade_.createShaderResourceBinding(true);
  }
}

void ScopeComputer::createBuffers() {
  auto *device = context_.device();

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
}

void ScopeComputer::updateParams(IDeviceContext *pContext, IBuffer *cb,
                                  int a, int b, int c, int d) {
  if (!cb) return;
  void *pData = nullptr;
  pContext->MapBuffer(cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
  if (pData) {
    int params[4] = {a, b, c, d};
    std::memcpy(pData, params, sizeof(params));
    pContext->UnmapBuffer(cb, MAP_WRITE);
  }
}

void ScopeComputer::computeVectorscope(IDeviceContext *pContext,
                                       ITextureView *inputTexture,
                                       IBuffer *outputVectorscope,
                                       int scopeSize, int step) {
  if (!ready()) return;

  updateParams(pContext, pVectorscopeParamsCB_, scopeSize, step, 0, 0);

  executorVectorscope_.setBuffer("VectorscopeParams", pVectorscopeParamsCB_);
  executorVectorscope_.setTextureView("g_InputTexture", inputTexture);
  executorVectorscope_.setBuffer("g_OutputVectorscope", outputVectorscope);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;
  uint32_t samplesX = (width + step - 1) / step;
  uint32_t samplesY = (height + step - 1) / step;
  uint32_t totalSampled = samplesX * samplesY;
  uint32_t groupCount =
      (totalSampled + THREAD_GROUP_SIZE * 16 - 1) / (THREAD_GROUP_SIZE * 16);

  executorVectorscope_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     THREAD_GROUP_SIZE));
}

void ScopeComputer::computeWaveform(IDeviceContext *pContext,
                                    ITextureView *inputTexture,
                                    IBuffer *outputWaveform,
                                    int outputWidth, int outputHeight,
                                    int step) {
  if (!ready()) return;

  updateParams(pContext, pWaveformParamsCB_, outputWidth, outputHeight, step, 0);

  executorWaveform_.setBuffer("WaveformParams", pWaveformParamsCB_);
  executorWaveform_.setTextureView("g_InputTexture", inputTexture);
  executorWaveform_.setBuffer("g_OutputWaveform", outputWaveform);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;
  uint32_t samplesX = (width + step - 1) / step;
  uint32_t samplesY = (height + step - 1) / step;
  uint32_t totalSampled = samplesX * samplesY;
  uint32_t groupCount =
      (totalSampled + THREAD_GROUP_SIZE * 16 - 1) / (THREAD_GROUP_SIZE * 16);

  executorWaveform_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     THREAD_GROUP_SIZE));
}

void ScopeComputer::computeParade(IDeviceContext *pContext,
                                  ITextureView *inputTexture,
                                  IBuffer *outputParade,
                                  int outputWidth, int outputHeight,
                                  int step) {
  if (!ready()) return;

  updateParams(pContext, pParadeParamsCB_, outputWidth, outputHeight, step, 0);

  executorParade_.setBuffer("ParadeParams", pParadeParamsCB_);
  executorParade_.setTextureView("g_InputTexture", inputTexture);
  executorParade_.setBuffer("g_OutputParade", outputParade);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;
  uint32_t samplesX = (width + step - 1) / step;
  uint32_t samplesY = (height + step - 1) / step;
  uint32_t totalSampled = samplesX * samplesY;
  uint32_t groupCount =
      (totalSampled + THREAD_GROUP_SIZE * 16 - 1) / (THREAD_GROUP_SIZE * 16);

  executorParade_.dispatch(
      pContext, ComputeExecutor::makeDispatchAttribs(groupCount, 1, 1,
                                                     THREAD_GROUP_SIZE));
}

bool ScopeComputer::readbackResults(IDeviceContext *pContext,
                                    IRenderDevice *pDevice,
                                    IBuffer *source,
                                    std::vector<uint32_t> &dest,
                                    size_t elementCount) {
  if (!source || elementCount == 0) return false;

  BufferDesc stagingDesc;
  stagingDesc.Name = "ScopeReadbackStaging";
  stagingDesc.Usage = USAGE_STAGING;
  stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
  stagingDesc.Size = static_cast<Uint64>(elementCount) * sizeof(uint32_t);
  stagingDesc.Mode = BUFFER_MODE_STRUCTURED;
  stagingDesc.ElementByteStride = sizeof(uint32_t);

  RefCntAutoPtr<IBuffer> pStaging;
  pDevice->CreateBuffer(stagingDesc, nullptr, &pStaging);
  if (!pStaging) return false;

  pContext->CopyBuffer(pStaging, source);
  pContext->Flush();
  pContext->FinishFrame();

  void *pData = nullptr;
  pContext->MapBuffer(pStaging, MAP_READ, MAP_FLAG_DO_NOT_WAIT, pData);
  if (!pData) {
    pContext->MapBuffer(pStaging, MAP_READ, MAP_FLAG_DO_NOT_WAIT, pData);
    if (!pData) return false;
  }

  dest.resize(elementCount);
  std::memcpy(dest.data(), pData, elementCount * sizeof(uint32_t));
  pContext->UnmapBuffer(pStaging, MAP_READ);

  return true;
}

bool ScopeComputer::ready() const {
  return executorVectorscope_.ready() && executorWaveform_.ready() &&
         executorParade_.ready();
}

} // namespace ArtifactCore
