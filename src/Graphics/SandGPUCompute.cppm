module;
#include <cstring>
#include <vector>
#include <cstdint>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>

module Graphics.SandGPUCompute;

import Graphics.GPUcomputeContext;
import Graphics.Compute;

namespace ArtifactCore {

using namespace Diligent;

// ============================================================================
// HLSL: Sand Simulation — cellular automaton update pass
// ============================================================================
static constexpr const char* g_SimulateHLSL = R"#(
RWTexture2D<uint> g_InputGrid  : register(u0);
RWTexture2D<uint> g_OutputGrid : register(u1);

cbuffer SandParams : register(b0) {
    uint g_Width;
    uint g_Height;
    uint g_Substep;
    uint g_Padding;
};

#define M_EMPTY 0
#define M_SAND  1
#define M_WATER 2
#define M_STONE 3
#define M_WOOD  4
#define M_FIRE  5
#define M_SMOKE 6
#define M_ACID  7

uint load(int2 pos) {
    if (pos.x < 0 || pos.x >= g_Width || pos.y < 0 || pos.y >= g_Height) return M_EMPTY;
    return g_InputGrid[pos];
}

void store(int2 pos, uint val) {
    if (pos.x >= 0 && pos.x < g_Width && pos.y >= 0 && pos.y < g_Height)
        g_OutputGrid[pos] = val;
}

uint hash3(uint x, uint y, uint z) {
    uint h = x * 0x9E3779B9u ^ y * 0x85EBCA6Bu ^ z * 0xC2B2AE35u;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

bool slideDiag(int2 pos, bool checkLeft) {
    int dx = checkLeft ? -1 : 1;
    if (load(pos + int2(dx, 1)) == M_EMPTY) {
        store(pos + int2(dx, 1), M_SAND);
        store(pos, M_EMPTY);
        return true;
    }
    return false;
}

[numthreads(16, 16, 1)]
void SimulateMain(uint3 DTid : SV_DispatchThreadID) {
    int2 p = int2(DTid.xy);
    if (p.x >= g_Width || p.y >= g_Height) return;

    uint mat = load(p);
    uint outMat = mat;
    uint rnd = hash3(p.x, p.y, g_Substep);

    [branch]
    switch (mat) {
    case M_SAND: {
        int2 below = p + int2(0, 1);
        uint bMat = load(below);
        if (bMat == M_EMPTY) {
            store(below, M_SAND);
            store(p, M_EMPTY);
            return;
        }
        if (bMat == M_WATER) {
            store(below, M_SAND);
            store(p, M_WATER);
            return;
        }
        if (bMat == M_ACID) {
            store(below, M_SAND);
            store(p, M_ACID);
            return;
        }
        bool leftFree  = p.x > 0       && load(p + int2(-1, 1)) == M_EMPTY;
        bool rightFree = p.x + 1 < g_Width && load(p + int2(1, 1)) == M_EMPTY;
        if (leftFree && rightFree) {
            if ((rnd & 1) == 0) { store(p + int2(-1, 1), M_SAND); store(p, M_EMPTY); }
            else                { store(p + int2(1, 1),  M_SAND); store(p, M_EMPTY); }
            return;
        } else if (leftFree)  { store(p + int2(-1, 1), M_SAND); store(p, M_EMPTY); return; }
        else if (rightFree)  { store(p + int2(1, 1),  M_SAND); store(p, M_EMPTY); return; }
        break;
    }
    case M_WATER: {
        int2 below = p + int2(0, 1);
        if (load(below) == M_EMPTY) {
            store(below, M_WATER); store(p, M_EMPTY); return;
        }
        bool leftD  = p.x > 0       && load(p + int2(-1, 1)) == M_EMPTY;
        bool rightD = p.x + 1 < g_Width && load(p + int2(1, 1))  == M_EMPTY;
        if (leftD && rightD) {
            if ((rnd & 1) == 0) { store(p + int2(-1, 1), M_WATER); store(p, M_EMPTY); }
            else                { store(p + int2(1, 1),  M_WATER); store(p, M_EMPTY); }
            return;
        } else if (leftD)  { store(p + int2(-1, 1), M_WATER); store(p, M_EMPTY); return; }
        else if (rightD)  { store(p + int2(1, 1),  M_WATER); store(p, M_EMPTY); return; }
        if ((rnd & 1) == 0) {
            if (p.x > 0            && load(p + int2(-1, 0)) == M_EMPTY) { store(p + int2(-1, 0), M_WATER); store(p, M_EMPTY); return; }
            if (p.x + 1 < g_Width  && load(p + int2(1, 0))  == M_EMPTY) { store(p + int2(1, 0),  M_WATER); store(p, M_EMPTY); return; }
        } else {
            if (p.x + 1 < g_Width  && load(p + int2(1, 0))  == M_EMPTY) { store(p + int2(1, 0),  M_WATER); store(p, M_EMPTY); return; }
            if (p.x > 0            && load(p + int2(-1, 0)) == M_EMPTY) { store(p + int2(-1, 0), M_WATER); store(p, M_EMPTY); return; }
        }
        break;
    }
    case M_FIRE: {
        if ((rnd & 3) == 0) { outMat = M_SMOKE; break; }
        if ((rnd & 7) == 0) { break; }
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int2 np = p + int2(dx, dy);
                if (load(np) == M_WOOD && (rnd & 3) == 0) store(np, M_FIRE);
            }
        int2 up = p + int2((rnd & 1) == 0 ? -1 : 1, -1);
        if (load(up) == M_EMPTY) { store(up, M_FIRE); store(p, M_EMPTY); return; }
        up = p + int2(0, -1);
        if (load(up) == M_EMPTY) { store(up, M_FIRE); store(p, M_EMPTY); return; }
        break;
    }
    case M_SMOKE: {
        if ((rnd & 3) == 0) { outMat = M_EMPTY; break; }
        int dx = (rnd & 1) == 0 ? -1 : 1;
        int2 up = p + int2(dx, -1);
        if (load(up) == M_EMPTY) { store(up, M_SMOKE); store(p, M_EMPTY); return; }
        up = p + int2(0, -1);
        if (load(up) == M_EMPTY) { store(up, M_SMOKE); store(p, M_EMPTY); return; }
        break;
    }
    case M_ACID: {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int2 np = p + int2(dx, dy);
                uint nm = load(np);
                if (nm != M_EMPTY && nm != M_STONE && nm != M_ACID && (rnd & 1) == 0)
                    store(np, M_EMPTY);
            }
        int2 below = p + int2(0, 1);
        if (load(below) == M_EMPTY) { store(below, M_ACID); store(p, M_EMPTY); return; }
        bool leftD  = p.x > 0       && load(p + int2(-1, 1)) == M_EMPTY;
        bool rightD = p.x + 1 < g_Width && load(p + int2(1, 1))  == M_EMPTY;
        if (leftD && rightD) {
            if ((rnd & 1) == 0) { store(p + int2(-1, 1), M_ACID); store(p, M_EMPTY); }
            else                { store(p + int2(1, 1),  M_ACID); store(p, M_EMPTY); }
            return;
        } else if (leftD)  { store(p + int2(-1, 1), M_ACID); store(p, M_EMPTY); return; }
        else if (rightD)  { store(p + int2(1, 1),  M_ACID); store(p, M_EMPTY); return; }
        if ((rnd & 1) == 0) {
            if (p.x > 0            && load(p + int2(-1, 0)) == M_EMPTY) { store(p + int2(-1, 0), M_ACID); store(p, M_EMPTY); return; }
            if (p.x + 1 < g_Width  && load(p + int2(1, 0))  == M_EMPTY) { store(p + int2(1, 0),  M_ACID); store(p, M_EMPTY); return; }
        } else {
            if (p.x + 1 < g_Width  && load(p + int2(1, 0))  == M_EMPTY) { store(p + int2(1, 0),  M_ACID); store(p, M_EMPTY); return; }
            if (p.x > 0            && load(p + int2(-1, 0)) == M_EMPTY) { store(p + int2(-1, 0), M_ACID); store(p, M_EMPTY); return; }
        }
        break;
    }
    default: break;
    }

    store(p, outMat);
}
)#";

// ============================================================================
// HLSL: Sand Render — convert grid to RGBA float4 output
// ============================================================================
static constexpr const char* g_RenderHLSL = R"#(
RWTexture2D<uint> g_Grid  : register(u0);
RWTexture2D<float4> g_Output : register(u1);

cbuffer SandParams : register(b0) {
    uint g_Width;
    uint g_Height;
    uint g_Substep;
    uint g_Padding;
};

#define M_EMPTY 0
#define M_SAND  1
#define M_WATER 2
#define M_STONE 3
#define M_WOOD  4
#define M_FIRE  5
#define M_SMOKE 6
#define M_ACID  7

float4 matColor(uint mat) {
    [forcecase] switch (mat) {
        case M_SAND:  return float4(0.76, 0.70, 0.50, 1.0);
        case M_WATER: return float4(0.0,  0.3,  0.8,  0.7);
        case M_STONE: return float4(0.5,  0.5,  0.5,  1.0);
        case M_WOOD:  return float4(0.55, 0.27, 0.07, 1.0);
        case M_FIRE: {
            uint h = g_Substep;
            float t = ((h >> 4) & 0xFF) / 255.0;
            return float4(1.0, t * 0.8 + 0.2, 0.0, 1.0);
        }
        case M_SMOKE: return float4(0.15, 0.15, 0.15, 0.5);
        case M_ACID:  return float4(0.0,  1.0,  0.2,  1.0);
        default:      return float4(0.0,  0.0,  0.0,  0.0);
    }
}

[numthreads(16, 16, 1)]
void RenderMain(uint3 DTid : SV_DispatchThreadID) {
    if (DTid.x >= g_Width || DTid.y >= g_Height) return;
    uint mat = g_Grid[DTid.xy];
    g_Output[DTid.xy] = matColor(mat);
}
)#";

// ============================================================================
// C++ Implementation
// ============================================================================

SandGPUCompute::SandGPUCompute(GpuContext& context)
    : context_(context), simulateExec_(context), renderExec_(context)
{
}

SandGPUCompute::~SandGPUCompute() = default;

bool SandGPUCompute::initialize(int width, int height) {
    width_ = width;
    height_ = height;
    pingPong_ = 0;

    if (!createTextures()) return false;
    if (!createConstantBuffer()) return false;
    if (!buildPipelines()) return false;

    ready_ = true;
    return true;
}

bool SandGPUCompute::createTextures() {
    auto* device = context_.RenderDevice();
    if (!device) return false;

    TextureDesc gridDesc;
    gridDesc.Type = RESOURCE_DIM_TEX_2D;
    gridDesc.Width = static_cast<Uint32>(width_);
    gridDesc.Height = static_cast<Uint32>(height_);
    gridDesc.Format = TEX_FORMAT_R32_UINT;
    gridDesc.Usage = USAGE_DEFAULT;
    gridDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    gridDesc.Name = "SandSim Grid";

    for (int i = 0; i < 2; ++i) {
        device->CreateTexture(gridDesc, nullptr, &pGridTex_[i]);
        if (!pGridTex_[i]) return false;
    }

    TextureDesc outDesc;
    outDesc.Type = RESOURCE_DIM_TEX_2D;
    outDesc.Width = static_cast<Uint32>(width_);
    outDesc.Height = static_cast<Uint32>(height_);
    outDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    outDesc.Usage = USAGE_DEFAULT;
    outDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    outDesc.Name = "SandSim Output";

    device->CreateTexture(outDesc, nullptr, &pOutputTex_);
    return pOutputTex_ != nullptr;
}

bool SandGPUCompute::createConstantBuffer() {
    auto* device = context_.RenderDevice();
    if (!device) return false;

    BufferDesc desc;
    desc.Name = "SandSim Params CB";
    desc.Size = sizeof(SimParams);
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(desc, nullptr, &pConstantBuffer_);
    return pConstantBuffer_ != nullptr;
}

bool SandGPUCompute::buildPipelines() {
    // --- Simulate pipeline ---
    {
        static const ShaderResourceVariableDesc simVars[] = {
            { SHADER_TYPE_COMPUTE, "g_InputGrid",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_OutputGrid", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "SandParams",   SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "SandSim Simulate";
        desc.shaderSource = g_SimulateHLSL;
        desc.entryPoint = "SimulateMain";
        desc.variables = simVars;
        desc.variableCount = 3;
        if (!simulateExec_.build(desc)) return false;
        if (!simulateExec_.createShaderResourceBinding(true)) return false;
        simulateExec_.setBuffer("SandParams", pConstantBuffer_);
    }

    // --- Render pipeline ---
    {
        static const ShaderResourceVariableDesc renVars[] = {
            { SHADER_TYPE_COMPUTE, "g_Grid",    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_Output",  SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "SandParams", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "SandSim Render";
        desc.shaderSource = g_RenderHLSL;
        desc.entryPoint = "RenderMain";
        desc.variables = renVars;
        desc.variableCount = 3;
        if (!renderExec_.build(desc)) return false;
        if (!renderExec_.createShaderResourceBinding(true)) return false;
        renderExec_.setBuffer("SandParams", pConstantBuffer_);
    }

    return true;
}

void SandGPUCompute::simulate(IDeviceContext* pContext, int substeps) {
    if (!ready_ || !pContext || width_ <= 0 || height_ <= 0) return;

    for (int step = 0; step < substeps; ++step) {
        params_.width = static_cast<Uint32>(width_);
        params_.height = static_cast<Uint32>(height_);
        params_.substep = static_cast<Uint32>(step);

        // Update constant buffer
        void* pData = nullptr;
        pContext->MapBuffer(pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        if (pData) {
            std::memcpy(pData, &params_, sizeof(params_));
            pContext->UnmapBuffer(pConstantBuffer_, MAP_WRITE);
        }

        int readIdx = pingPong_;
        int writeIdx = 1 - pingPong_;

        auto* readUAV = pGridTex_[readIdx]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
        auto* writeUAV = pGridTex_[writeIdx]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);

        simulateExec_.setTextureView("g_InputGrid", readUAV);
        simulateExec_.setTextureView("g_OutputGrid", writeUAV);

        auto attribs = ComputeExecutor::makeDispatchAttribs(
            static_cast<Uint32>(width_), static_cast<Uint32>(height_), 1, 16, 16, 1);
        simulateExec_.dispatch(pContext, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pingPong_ = writeIdx;
    }
}

void SandGPUCompute::renderToOutput(IDeviceContext* pContext, ITextureView* outputUAV) {
    if (!ready_ || !pContext || !outputUAV) return;

    params_.width = static_cast<Uint32>(width_);
    params_.height = static_cast<Uint32>(height_);

    void* pData = nullptr;
    pContext->MapBuffer(pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        std::memcpy(pData, &params_, sizeof(params_));
        pContext->UnmapBuffer(pConstantBuffer_, MAP_WRITE);
    }

    auto* gridSRV = pGridTex_[pingPong_]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    auto* gridUAV = pGridTex_[pingPong_]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);

    renderExec_.setTextureView("g_Grid", gridUAV);
    renderExec_.setTextureView("g_Output", outputUAV);

    auto attribs = ComputeExecutor::makeDispatchAttribs(
        static_cast<Uint32>(width_), static_cast<Uint32>(height_), 1, 16, 16, 1);
    renderExec_.dispatch(pContext, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void SandGPUCompute::uploadFromCPU(IDeviceContext* pContext,
    const std::vector<uint8_t>& grid, int width, int height)
{
    if (!pContext || width <= 0 || height <= 0) return;
    if (width != width_ || height != height_ || grid.size() < static_cast<size_t>(width * height)) return;

    TextureDesc stagingDesc;
    stagingDesc.Type = RESOURCE_DIM_TEX_2D;
    stagingDesc.Width = static_cast<Uint32>(width);
    stagingDesc.Height = static_cast<Uint32>(height);
    stagingDesc.Format = TEX_FORMAT_R32_UINT;
    stagingDesc.Usage = USAGE_IMMUTABLE;
    stagingDesc.BindFlags = BIND_SHADER_RESOURCE;
    stagingDesc.Name = "SandSim Staging Upload";

    TextureSubResData subData;
    subData.pData = grid.data();
    subData.Stride = static_cast<Uint64>(width) * sizeof(uint32_t);

    TextureData initData;
    initData.pSubResources = &subData;
    initData.NumSubresources = 1;

    RefCntAutoPtr<ITexture> staging;
    context_.RenderDevice()->CreateTexture(stagingDesc, &initData, &staging);
    if (!staging) return;

    CopyTextureAttribs copy(staging, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            pGridTex_[pingPong_], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CopyTexture(copy);

    // Also zero the other buffer by copying from a zero-filled staging texture.
    std::vector<uint8_t> zeroGrid(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0);
    TextureSubResData zeroSubData;
    zeroSubData.pData = zeroGrid.data();
    zeroSubData.Stride = static_cast<Uint64>(width_) * sizeof(uint8_t);

    TextureData zeroInitData;
    zeroInitData.pSubResources = &zeroSubData;
    zeroInitData.NumSubresources = 1;

    RefCntAutoPtr<ITexture> zeroStaging;
    context_.RenderDevice()->CreateTexture(stagingDesc, &zeroInitData, &zeroStaging);
    if (zeroStaging) {
        CopyTextureAttribs zeroCopy(zeroStaging, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                   pGridTex_[1 - pingPong_], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->CopyTexture(zeroCopy);
    }
}

void SandGPUCompute::readbackToCPU(IDeviceContext* pContext, std::vector<uint8_t>& grid) {
    if (!pContext || width_ <= 0 || height_ <= 0) return;
    grid.resize(static_cast<size_t>(width_ * height_));

    TextureDesc stagingDesc;
    stagingDesc.Type = RESOURCE_DIM_TEX_2D;
    stagingDesc.Width = static_cast<Uint32>(width_);
    stagingDesc.Height = static_cast<Uint32>(height_);
    stagingDesc.Format = TEX_FORMAT_R32_UINT;
    stagingDesc.Usage = USAGE_STAGING;
    stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    stagingDesc.Name = "SandSim Staging Readback";

    RefCntAutoPtr<ITexture> staging;
    context_.RenderDevice()->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) return;

    CopyTextureAttribs copy(pGridTex_[pingPong_], RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            staging, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CopyTexture(copy);
    pContext->Flush();
    pContext->WaitForIdle();

    MappedTextureSubresource mapped{};
    pContext->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);

    const auto* src = static_cast<const uint8_t*>(mapped.pData);
    for (int y = 0; y < height_; ++y) {
        std::memcpy(&grid[static_cast<size_t>(y) * width_],
                     src + static_cast<size_t>(y) * mapped.Stride,
                     static_cast<size_t>(width_));
    }

    pContext->UnmapTextureSubresource(staging, 0, 0);
}

ITextureView* SandGPUCompute::gridSRV() const {
    if (!ready_) return nullptr;
    return pGridTex_[pingPong_]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
}

} // namespace ArtifactCore
