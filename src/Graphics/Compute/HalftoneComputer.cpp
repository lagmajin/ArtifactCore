module;
#include <cmath>
#include <cstring>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Compute.HalftoneComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.Halftone;

namespace ArtifactCore {

using namespace Diligent;

HalftoneGPUComputer::HalftoneGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

HalftoneGPUComputer::~HalftoneGPUComputer() = default;

void HalftoneGPUComputer::initialize() {
    if (!context_.RenderDevice()) return;
    createPipeline();
    createBuffers();
}

void HalftoneGPUComputer::createPipeline() {
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "HalftoneParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ComputePipelineDesc desc;
    desc.name = "Halftone/Apply";
    desc.shaderSource = Shaders::Halftone::HalftoneSource;
    desc.entryPoint = Shaders::Halftone::HalftoneEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 3;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executor_.build(desc);
    executor_.createShaderResourceBinding(true);
}

void HalftoneGPUComputer::createBuffers() {
    auto *device = context_.RenderDevice();
    if (!device) return;
    BufferDesc desc;
    desc.Name = "HalftoneParamsCB";
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.Size = 48;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(desc, nullptr, &pParamsCB_);
}

void HalftoneGPUComputer::apply(IDeviceContext *pContext,
                                 ITextureView *inputTexture,
                                 ITextureView *outputTexture,
                                 const HalftoneGPUParams &params) {
    if (!ready() || !pContext || !inputTexture || !outputTexture ||
        !inputTexture->GetTexture() || !outputTexture->GetTexture()) return;

    const auto inputDesc = inputTexture->GetTexture()->GetDesc();
    const auto outputDesc = outputTexture->GetTexture()->GetDesc();
    if (inputDesc.Width == 0 || inputDesc.Height == 0 ||
        outputDesc.Width == 0 || outputDesc.Height == 0 ||
        inputDesc.Width != outputDesc.Width || inputDesc.Height != outputDesc.Height) return;

    void *pData = nullptr;
    pContext->MapBuffer(pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData) return;

    struct HalftoneConstantBuffer {
        float dotSize, angleRad, contrast, ellipseAspect;
        int colorMode, dotShape, pad0, pad1;
        float cmykAngles[4];
    } cb{};
    static_assert(sizeof(HalftoneConstantBuffer) == 48);
    cb.dotSize = params.dotSize;
    cb.angleRad = params.angleDeg * 3.14159265f / 180.0f;
    cb.contrast = params.contrast;
    cb.ellipseAspect = params.ellipseAspect;
    cb.colorMode = params.colorMode;
    cb.dotShape = params.dotShape;
    std::memcpy(cb.cmykAngles, params.cmykAngles, sizeof(float) * 4);
    std::memcpy(pData, &cb, sizeof(cb));
    pContext->UnmapBuffer(pParamsCB_, MAP_WRITE);

    if (!executor_.setBuffer("HalftoneParams", pParamsCB_) ||
        !executor_.setTextureView("g_InputTexture", inputTexture) ||
        !executor_.setTextureView("g_OutputTexture", outputTexture)) return;

    executor_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(
        outputDesc.Width, outputDesc.Height, 1, 16, 16, 1));
}

bool HalftoneGPUComputer::ready() const { return executor_.ready() && pParamsCB_; }

} // namespace ArtifactCore
