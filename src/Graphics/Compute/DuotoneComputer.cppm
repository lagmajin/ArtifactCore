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

DuotoneGPUComputer::DuotoneGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

DuotoneGPUComputer::~DuotoneGPUComputer() = default;

void DuotoneGPUComputer::initialize() {
    createPipeline();
    createBuffers();
}

void DuotoneGPUComputer::createPipeline() {
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ComputePipelineDesc desc;
    desc.name = "Duotone/Apply";
    desc.shaderSource = Shaders::Duotone::DuotoneSource;
    desc.entryPoint = Shaders::Duotone::DuotoneEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 2;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executor_.build(desc);
    executor_.createShaderResourceBinding(true);
}

void DuotoneGPUComputer::createBuffers() {
    auto *device = context_.device();
    BufferDesc desc;
    desc.Name = "DuotoneParamsCB";
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.Size = sizeof(float) * 8 + sizeof(float); // shadow(4) + highlight(4) + blend(1), padded to 16 bytes
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(desc, nullptr, &pParamsCB_);
}

void DuotoneGPUComputer::apply(IDeviceContext *pContext,
                                ITextureView *inputTexture,
                                ITextureView *outputTexture,
                                const DuotoneGPUParams &params) {
    if (!ready() || !inputTexture || !outputTexture) return;

    void *pData = nullptr;
    pContext->MapBuffer(pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        struct { float shadow[4]; float highlight[4]; float blend; float pad[3]; } cb;
        std::memcpy(cb.shadow, params.shadowColor, sizeof(float) * 4);
        std::memcpy(cb.highlight, params.highlightColor, sizeof(float) * 4);
        cb.blend = params.blend;
        cb.pad[0] = cb.pad[1] = cb.pad[2] = 0;
        std::memcpy(pData, &cb, sizeof(cb));
        pContext->UnmapBuffer(pParamsCB_, MAP_WRITE);
    }

    executor_.setBuffer("DuotoneParams", pParamsCB_);
    executor_.setTextureView("g_InputTexture", inputTexture);
    executor_.setTextureView("g_OutputTexture", outputTexture);

    const auto w = outputTexture->GetTexture()->GetDesc().Width;
    const auto h = outputTexture->GetTexture()->GetDesc().Height;
    executor_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
}

bool DuotoneGPUComputer::ready() const { return executor_.ready() && pParamsCB_; }

} // namespace ArtifactCore
