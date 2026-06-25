module;
#include <cstring>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Graphics.Compute.EdgeEchoComputer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.Shader.Compute.HLSL.EdgeEcho;

export namespace ArtifactCore {

using namespace Diligent;

EdgeEchoGPUComputer::EdgeEchoGPUComputer(GpuContext &context)
    : context_(context),
      executorSobel_(context),
      executorWarp_(context),
      executorComposite_(context) {}

EdgeEchoGPUComputer::~EdgeEchoGPUComputer() = default;

void EdgeEchoGPUComputer::initialize() {
    createPipelines();
    createBuffers();
}

void EdgeEchoGPUComputer::createPipelines() {
    // Sobel: input texture → edge texture
    {
        static ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_COMPUTE, "g_InputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_OutputEdges",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        ComputePipelineDesc desc;
        desc.name = "EdgeEcho/Sobel";
        desc.shaderSource = Shaders::EdgeEcho::SobelSource;
        desc.entryPoint = Shaders::EdgeEcho::SobelEntryPoint;
        desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        desc.variables = vars;
        desc.variableCount = 2;
        desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        executorSobel_.build(desc);
        executorSobel_.createShaderResourceBinding(true);
    }
    // Warp: edges + history → new history
    {
        static ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_COMPUTE, "g_CurrentEdges",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_History",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_OutputHistory", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        ComputePipelineDesc desc;
        desc.name = "EdgeEcho/Warp";
        desc.shaderSource = Shaders::EdgeEcho::WarpSource;
        desc.entryPoint = Shaders::EdgeEcho::WarpEntryPoint;
        desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        desc.variables = vars;
        desc.variableCount = 3;
        desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        executorWarp_.build(desc);
        executorWarp_.createShaderResourceBinding(true);
    }
    // Composite: original + history → output
    {
        static ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_COMPUTE, "g_InputTexture",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_History",       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        ComputePipelineDesc desc;
        desc.name = "EdgeEcho/Composite";
        desc.shaderSource = Shaders::EdgeEcho::CompositeSource;
        desc.entryPoint = Shaders::EdgeEcho::CompositeEntryPoint;
        desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        desc.variables = vars;
        desc.variableCount = 3;
        desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        executorComposite_.build(desc);
        executorComposite_.createShaderResourceBinding(true);
    }
}

void EdgeEchoGPUComputer::createBuffers() {
    auto *device = context_.device();

    auto makeCB = [&](const char* name, size_t size) -> RefCntAutoPtr<IBuffer> {
        BufferDesc desc;
        desc.Name = name;
        desc.Usage = USAGE_DYNAMIC;
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.Size = static_cast<Uint64>(size);
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        RefCntAutoPtr<IBuffer> buf;
        device->CreateBuffer(desc, nullptr, &buf);
        return buf;
    };

    pSobelParamsCB_ = makeCB("EdgeEchoSobelParams", sizeof(float) * 4);
    pWarpParamsCB_ = makeCB("EdgeEchoWarpParams", sizeof(float) * 4);
    pCompositeParamsCB_ = makeCB("EdgeEchoCompositeParams", sizeof(float) * 4);
}

void EdgeEchoGPUComputer::ensureTextures(IDeviceContext *pContext, int width, int height) {
    if (width == lastWidth_ && height == lastHeight_ && pEdgeTexture_) return;

    auto *device = context_.device();
    lastWidth_ = width;
    lastHeight_ = height;
    hasHistory_ = false;

    // Edge texture (single channel R32_FLOAT)
    {
        TextureDesc desc;
        desc.Name = "EdgeEcho_Edges";
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Uint32>(width);
        desc.Height = static_cast<Uint32>(height);
        desc.Format = TEX_FORMAT_R32_FLOAT;
        desc.Usage = USAGE_DEFAULT;
        desc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
        device->CreateTexture(desc, nullptr, &pEdgeTexture_);
        pEdgeUAV_ = pEdgeTexture_->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    }

    // History texture (RGBA32_FLOAT)
    {
        TextureDesc desc;
        desc.Name = "EdgeEcho_History";
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Uint32>(width);
        desc.Height = static_cast<Uint32>(height);
        desc.Format = TEX_FORMAT_R32G32B32A32_FLOAT;
        desc.Usage = USAGE_DEFAULT;
        desc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;

        // Initialize to black
        float clear[4] = {0, 0, 0, 0};
        TextureSubResData initData;
        initData.pData = clear;
        initData.Stride = sizeof(float) * 4 * width;
        TextureData texData;
        texData.pSubResources = &initData;
        texData.NumSubresources = 1;
        // Can't init full texture with one pixel - use separate clear
        device->CreateTexture(desc, nullptr, &pHistoryTexture_);
        pHistorySRV_ = pHistoryTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        pHistoryUAV_ = pHistoryTexture_->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
    }
}

void EdgeEchoGPUComputer::apply(IDeviceContext *pContext,
                                 ITextureView *inputTexture,
                                 ITextureView *outputTexture,
                                 float edgeThreshold,
                                 float decay,
                                 float waveFreq,
                                 float waveAmp,
                                 float timeEvolution,
                                 const float* echoColor) {
    if (!ready() || !inputTexture || !outputTexture) return;

    const auto w = inputTexture->GetTexture()->GetDesc().Width;
    const auto h = inputTexture->GetTexture()->GetDesc().Height;
    ensureTextures(pContext, static_cast<int>(w), static_cast<int>(h));
    if (!pEdgeUAV_ || !pHistoryUAV_) return;

    // ---- Pass 1: Sobel ----
    {
        void *pData = nullptr;
        pContext->MapBuffer(pSobelParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        if (pData) {
            float params[4] = {edgeThreshold, 0, 0, 0};
            std::memcpy(pData, params, sizeof(params));
            pContext->UnmapBuffer(pSobelParamsCB_, MAP_WRITE);
        }
        executorSobel_.setBuffer("SobelParams", pSobelParamsCB_);
        executorSobel_.setTextureView("g_InputTexture", inputTexture);
        executorSobel_.setTextureView("g_OutputEdges", pEdgeUAV_);
        executorSobel_.dispatch(pContext,
            ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
    }

    // ---- Pass 2: Warp ----
    {
        void *pData = nullptr;
        pContext->MapBuffer(pWarpParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        if (pData) {
            float params[4] = {decay, waveAmp, waveFreq, timeEvolution};
            std::memcpy(pData, params, sizeof(params));
            pContext->UnmapBuffer(pWarpParamsCB_, MAP_WRITE);
        }
        executorWarp_.setBuffer("WarpParams", pWarpParamsCB_);
        executorWarp_.setTextureView("g_CurrentEdges", pEdgeUAV_);
        executorWarp_.setTextureView("g_History", hasHistory_ ? pHistorySRV_ : pEdgeUAV_);
        executorWarp_.setTextureView("g_OutputHistory", pHistoryUAV_);
        executorWarp_.dispatch(pContext,
            ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
    }
    hasHistory_ = true;

    // ---- Pass 3: Composite ----
    {
        void *pData = nullptr;
        pContext->MapBuffer(pCompositeParamsCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        if (pData) {
            float params[4] = {echoColor[0], echoColor[1], echoColor[2], echoColor[3]};
            std::memcpy(pData, params, sizeof(params));
            pContext->UnmapBuffer(pCompositeParamsCB_, MAP_WRITE);
        }
        executorComposite_.setBuffer("CompositeParams", pCompositeParamsCB_);
        executorComposite_.setTextureView("g_InputTexture", inputTexture);
        executorComposite_.setTextureView("g_History", pHistorySRV_);
        executorComposite_.setTextureView("g_OutputTexture", outputTexture);
        executorComposite_.dispatch(pContext,
            ComputeExecutor::makeDispatchAttribs(w, h, 1, 16, 16, 1));
    }
}

void EdgeEchoGPUComputer::reset() {
    hasHistory_ = false;
}

bool EdgeEchoGPUComputer::ready() const {
    return executorSobel_.ready() && executorWarp_.ready() && executorComposite_.ready()
           && pSobelParamsCB_ && pWarpParamsCB_ && pCompositeParamsCB_;
}

} // namespace ArtifactCore
