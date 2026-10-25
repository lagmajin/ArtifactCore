module;
#include <cstring>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Compute.DuotoneComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.Duotone;

namespace ArtifactCore {

using namespace Diligent;

namespace {

struct DuotoneConstantBuffer {
    float shadow[4];
    float highlight[4];
    float blend;
    float padding[3];
};

static_assert(sizeof(DuotoneConstantBuffer) == 48,
              "Duotone constant buffer must match the HLSL layout");

}

DuotoneGPUComputer::DuotoneGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

DuotoneGPUComputer::~DuotoneGPUComputer() = default;

void DuotoneGPUComputer::initialize() {
    if (!context_.RenderDevice()) {
        return;
    }
    createPipeline();
    createBuffers();
}

void DuotoneGPUComputer::createPipeline() {
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "DuotoneParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ComputePipelineDesc desc;
    desc.name = "Duotone/Apply";
    desc.shaderSource = Shaders::Duotone::DuotoneSource;
    desc.entryPoint = Shaders::Duotone::DuotoneEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 3;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executor_.build(desc);
    executor_.createShaderResourceBinding(true);
}

void DuotoneGPUComputer::createBuffers() {
  auto *device = context_.RenderDevice();
    if (!device) {
        return;
    }
    BufferDesc desc;
    desc.Name = "DuotoneParamsCB";
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.Size = sizeof(DuotoneConstantBuffer);
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(desc, nullptr, &pParamsCB_);
}

void DuotoneGPUComputer::apply(IDeviceContext *pContext,
                                ITextureView *inputTexture,
                                ITextureView *outputTexture,
                                const DuotoneGPUParams &params) {
    if (!ready() || !pContext || !inputTexture || !inputTexture->GetTexture() ||
        !outputTexture || !outputTexture->GetTexture()) {
        return;
    }

    void *pData = nullptr;
    pContext->MapBuffer(pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData) {
        return;
    }
    DuotoneConstantBuffer cb{};
    std::memcpy(cb.shadow, params.shadowColor, sizeof(cb.shadow));
    std::memcpy(cb.highlight, params.highlightColor, sizeof(cb.highlight));
    cb.blend = params.blend;
    std::memcpy(pData, &cb, sizeof(cb));
    pContext->UnmapBuffer(pParamsCB_, MAP_WRITE);

    if (!executor_.setBuffer("DuotoneParams", pParamsCB_) ||
        !executor_.setTextureView("g_InputTexture", inputTexture) ||
        !executor_.setTextureView("g_OutputTexture", outputTexture)) {
        return;
    }

    const auto w = outputTexture->GetTexture()->GetDesc().Width;
    const auto h = outputTexture->GetTexture()->GetDesc().Height;
    executor_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
}

bool DuotoneGPUComputer::ready() const { return executor_.ready() && pParamsCB_; }

} // namespace ArtifactCore
