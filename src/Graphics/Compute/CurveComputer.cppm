module;
#include <array>
#include <cstring>
#include <cstdint>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Compute.CurveComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.ColorCurves;

namespace ArtifactCore {

using namespace Diligent;

CurveGPUComputer::CurveGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

CurveGPUComputer::~CurveGPUComputer() = default;

void CurveGPUComputer::initialize() {
  createPipeline();
  createBuffers();
}

void CurveGPUComputer::createPipeline() {
  static ShaderResourceVariableDesc vars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_LUTs",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  ComputePipelineDesc desc;
  desc.name = "Curves/Apply";
  desc.shaderSource = Shaders::ColorCurves::ColorCurvesSource;
  desc.entryPoint = Shaders::ColorCurves::ColorCurvesEntryPoint;
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = vars;
  desc.variableCount = 3;
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  executor_.build(desc);
  executor_.createShaderResourceBinding(true);
}

void CurveGPUComputer::createBuffers() {
  auto *device = context_.device();

  BufferDesc cbDesc;
  cbDesc.Name = "CurveParamsCB";
  cbDesc.Usage = USAGE_DYNAMIC;
  cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
  cbDesc.Size = sizeof(int) * 4;
  cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  device->CreateBuffer(cbDesc, nullptr, &pParamsCB_);

  BufferDesc lutDesc;
  lutDesc.Name = "CurveLUTs";
  lutDesc.Usage = USAGE_DYNAMIC;
  lutDesc.BindFlags = BIND_SHADER_RESOURCE;
  lutDesc.Size = static_cast<Uint64>(Shaders::ColorCurves::LUT_TOTAL_FLOATS) * sizeof(float);
  lutDesc.Mode = BUFFER_MODE_STRUCTURED;
  lutDesc.ElementByteStride = sizeof(float);
  device->CreateBuffer(lutDesc, nullptr, &pLUTBuffer_);
}

void CurveGPUComputer::apply(IDeviceContext *pContext,
                              ITextureView *inputTexture,
                              ITextureView *outputTexture,
                              const float *luts,
                              bool masterOnly) {
  if (!ready() || !inputTexture || !outputTexture || !luts) return;

  // Write masterOnly flag
  void *pData = nullptr;
  pContext->MapBuffer(pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
  if (pData) {
    int params[4] = {masterOnly ? 1 : 0, 0, 0, 0};
    std::memcpy(pData, params, sizeof(params));
    pContext->UnmapBuffer(pParamsCB_, MAP_WRITE);
  }

  // Upload LUT data
  pContext->MapBuffer(pLUTBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
  if (pData) {
    std::memcpy(pData, luts,
                static_cast<size_t>(Shaders::ColorCurves::LUT_TOTAL_FLOATS) * sizeof(float));
    pContext->UnmapBuffer(pLUTBuffer_, MAP_WRITE);
  }

  executor_.setBuffer("CurveParams", pParamsCB_);
  executor_.setBuffer("g_LUTs", pLUTBuffer_);
  executor_.setTextureView("g_InputTexture", inputTexture);
  executor_.setTextureView("g_OutputTexture", outputTexture);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;

  executor_.dispatch(pContext,
      ComputeExecutor::makeDispatchAttribs(width, height, 1, 16, 16, 1));
}

bool CurveGPUComputer::ready() const {
  return executor_.ready() && pParamsCB_ && pLUTBuffer_;
}

} // namespace ArtifactCore
