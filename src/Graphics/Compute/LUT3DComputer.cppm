module;
#include <cstring>
#include <vector>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Compute.LUT3DComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.LUT3D;

namespace ArtifactCore {

using namespace Diligent;

LUT3DGPUComputer::LUT3DGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

LUT3DGPUComputer::~LUT3DGPUComputer() = default;

void LUT3DGPUComputer::initialize() {
  createPipeline();
}

void LUT3DGPUComputer::createPipeline() {
  static ShaderResourceVariableDesc vars[] = {
      {SHADER_TYPE_COMPUTE, "g_InputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_LUT",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_OutputTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  ComputePipelineDesc desc;
  desc.name = "LUT3D/Apply";
  desc.shaderSource = Shaders::LUT3D::LUT3DSource;
  desc.entryPoint = Shaders::LUT3D::LUT3DEntryPoint;
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = vars;
  desc.variableCount = 3;
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  executor_.build(desc);
  executor_.createShaderResourceBinding(true);
}

void LUT3DGPUComputer::uploadLUT(IDeviceContext *pContext,
                                  const float *data, int size) {
  if (!data || size < 2 || size > 256) return;

  auto *device = context_.RenderDevice();
  if (!device) {
    return;
  }
  lutSize_ = size;

  // Build RGBA texels from RGB input
  int totalVoxels = size * size * size;
  std::vector<float> rgba(static_cast<size_t>(totalVoxels) * 4);
  for (int i = 0; i < totalVoxels; ++i) {
    rgba[i * 4 + 0] = data[i * 3 + 0];
    rgba[i * 4 + 1] = data[i * 3 + 1];
    rgba[i * 4 + 2] = data[i * 3 + 2];
    rgba[i * 4 + 3] = 1.0f;
  }

  Box box;
  box.MinX = 0; box.MinY = 0; box.MinZ = 0;
  box.MaxX = size; box.MaxY = size; box.MaxZ = size;

  TextureSubResData subRes;
  subRes.pData = rgba.data();
  subRes.Stride = static_cast<Uint64>(size) * 4 * sizeof(float);
  subRes.DepthStride = subRes.Stride * static_cast<Uint64>(size);

  TextureData texData;
  texData.pSubResources = &subRes;
  texData.NumSubresources = 1;

  TextureDesc texDesc;
  texDesc.Name = "LUT3D";
  texDesc.Type = RESOURCE_DIM_TEX_3D;
  texDesc.Width = static_cast<Uint32>(size);
  texDesc.Height = static_cast<Uint32>(size);
  texDesc.Depth = static_cast<Uint32>(size);
  texDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
  texDesc.Usage = USAGE_IMMUTABLE;
  texDesc.BindFlags = BIND_SHADER_RESOURCE;

  device->CreateTexture(texDesc, &texData, &pLUTTexture_);
  if (pLUTTexture_) {
    pLUTSRV_ = pLUTTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
  }
}

void LUT3DGPUComputer::apply(IDeviceContext *pContext,
                              ITextureView *inputTexture,
                              ITextureView *outputTexture) {
  if (!ready() || !inputTexture || !outputTexture || !hasLUT()) return;

  executor_.setTextureView("g_InputTexture", inputTexture);
  executor_.setTextureView("g_LUT", pLUTSRV_);
  executor_.setTextureView("g_OutputTexture", outputTexture);

  const auto width = inputTexture->GetTexture()->GetDesc().Width;
  const auto height = inputTexture->GetTexture()->GetDesc().Height;

  executor_.dispatch(pContext,
      ComputeExecutor::makeDispatchAttribs(width, height, 1, 16, 16, 1));
}

bool LUT3DGPUComputer::hasLUT() const { return pLUTSRV_ != nullptr; }
bool LUT3DGPUComputer::ready() const { return executor_.ready(); }
int LUT3DGPUComputer::lutSize() const { return lutSize_; }

} // namespace ArtifactCore
