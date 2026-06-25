module;
#include <cmath>
#include <cstring>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Graphics.Compute.HalftoneComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.Halftone;

export namespace ArtifactCore {

using namespace Diligent;

HalftoneGPUComputer::HalftoneGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

HalftoneGPUComputer::~HalftoneGPUComputer() = default;

void HalftoneGPUComputer::initialize() {
    createPipeline();
    createBuffers();
}

void HalftoneGPUComputer::createPipeline() {
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ComputePipelineDesc desc;
    desc.name = "Halftone/Apply";
    desc.shaderSource = Shaders::Halftone::HalftoneSource;
    desc.entryPoint = Shaders::Halftone::HalftoneEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 2;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executor_.build(desc);
    executor_.createShaderResourceBinding(true);
}

void HalftoneGPUComputer::createBuffers() {
    auto *device = context_.device();
    BufferDesc desc;
    desc.Name = "HalftoneParamsCB";
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.Size = sizeof(float) * 12 + sizeof(int) * 4;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(desc, nullptr, &pParamsCB_);
}

void HalftoneGPUComputer::apply(IDeviceContext *pContext,
                                 ITextureView *inputTexture,
                                 ITextureView *outputTexture,
                                 const HalftoneGPUParams &params) {
    if (!ready() || !inputTexture || !outputTexture) return;

    void *pData = nullptr;
    pContext->MapBuffer(pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        struct {
            float dotSize, angleRad, contrast; int colorMode;
            float ellipseAspect, cmykAngles[4]; int dotShape; int pad;
        } cb;
        cb.dotSize = params.dotSize;
        cb.angleRad = params.angleDeg * 3.14159265f / 180.0f;
        cb.contrast = params.contrast;
        cb.colorMode = params.colorMode;
        cb.ellipseAspect = params.ellipseAspect;
        std::memcpy(cb.cmykAngles, params.cmykAngles, sizeof(float) * 4);
        cb.dotShape = params.dotShape;
        cb.pad = 0;
        std::memcpy(pData, &cb, sizeof(cb));
        pContext->UnmapBuffer(pParamsCB_, MAP_WRITE);
    }

    executor_.setBuffer("HalftoneParams", pParamsCB_);
    executor_.setTextureView("g_InputTexture", inputTexture);
    executor_.setTextureView("g_OutputTexture", outputTexture);

    const auto w = outputTexture->GetTexture()->GetDesc().Width;
    const auto h = outputTexture->GetTexture()->GetDesc().Height;
    executor_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
}

bool HalftoneGPUComputer::ready() const { return executor_.ready() && pParamsCB_; }

} // namespace ArtifactCore
