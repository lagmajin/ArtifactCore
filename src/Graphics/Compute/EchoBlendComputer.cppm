module;
#include <cmath>
#include <cstring>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Compute.EchoBlendComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.EchoBlend;

namespace ArtifactCore {

using namespace Diligent;

EchoBlendGPUComputer::EchoBlendGPUComputer(GpuContext &context)
    : context_(context), executor_(context) {}

EchoBlendGPUComputer::~EchoBlendGPUComputer() = default;

void EchoBlendGPUComputer::initialize() {
    createPipeline();
    createBuffers();
}

void EchoBlendGPUComputer::createPipeline() {
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring0", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring1", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring2", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring3", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring4", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring5", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring6", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Ring7", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    ComputePipelineDesc desc;
    desc.name = "EchoBlend/Apply";
    desc.shaderSource = Shaders::EchoBlend::EchoBlendSource;
    desc.entryPoint = Shaders::EchoBlend::EchoBlendEntryPoint;
    desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 10;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    executor_.build(desc);
    executor_.createShaderResourceBinding(true);
}

void EchoBlendGPUComputer::createBuffers() {
    auto *device = context_.device();
    BufferDesc desc;
    desc.Name = "EchoBlendParamsCB";
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.Size = sizeof(int) + sizeof(float) * 3;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(desc, nullptr, &pParamsCB_);
}

void EchoBlendGPUComputer::bindRingEntries(IDeviceContext *pContext,
                                            const std::vector<ITextureView*> &ring) {
    const char* names[8] = {
        "g_Ring0", "g_Ring1", "g_Ring2", "g_Ring3",
        "g_Ring4", "g_Ring5", "g_Ring6", "g_Ring7"
    };
    for (int i = 0; i < 8; ++i) {
        if (i < static_cast<int>(ring.size()) && ring[i]) {
            executor_.setTextureView(names[i], ring[i]);
        }
    }
}

void EchoBlendGPUComputer::apply(IDeviceContext *pContext,
                                  ITextureView *inputTexture,
                                  ITextureView *outputTexture,
                                  const std::vector<ITextureView*> &ringTextures,
                                  int frameCount,
                                  float decay,
                                  float startingIntensity) {
    if (!ready() || !outputTexture || ringTextures.empty()) return;

    // Compute normalizer: same as CPU code
    float normSum = 1.0f;
    if (frameCount > 1) {
        normSum = 1.0f + startingIntensity * (1.0f - std::pow(decay, static_cast<float>(frameCount - 1)))
                  / std::max(1.0f - decay, 0.001f);
    }
    float invNormalizer = 1.0f / normSum;

    void *pData = nullptr;
    pContext->MapBuffer(pParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        struct { int frameCount; float invNorm; float decay; float startInt; } cb;
        cb.frameCount = frameCount;
        cb.invNorm = invNormalizer;
        cb.decay = decay;
        cb.startInt = startingIntensity;
        std::memcpy(pData, &cb, sizeof(cb));
        pContext->UnmapBuffer(pParamsCB_, MAP_WRITE);
    }

    executor_.setBuffer("EchoParams", pParamsCB_);
    executor_.setTextureView("g_InputTexture", inputTexture ? inputTexture : ringTextures[0]);
    executor_.setTextureView("g_OutputTexture", outputTexture);
    bindRingEntries(pContext, ringTextures);

    const auto w = outputTexture->GetTexture()->GetDesc().Width;
    const auto h = outputTexture->GetTexture()->GetDesc().Height;
    executor_.dispatch(pContext, ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
}

bool EchoBlendGPUComputer::ready() const { return executor_.ready() && pParamsCB_; }

} // namespace ArtifactCore
