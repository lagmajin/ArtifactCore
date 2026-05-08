module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>
#include <QDebug>

module Graphics.ParticleRenderer;

import std;
import Frame.Debug;

namespace ArtifactCore {

struct ParticleRenderer::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pSRB_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pParticleBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pConstantBuffer_;
};

const char* ParticleVSSource = R"(
struct ParticleData {
    float3 position;
    float3 velocity;
    float4 color;
    float  size;
    float  rotation;
    float  age;
    float  lifetime;
};

StructuredBuffer<ParticleData> g_Particles : register(t0);

cbuffer Constants : register(b0) {
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float DeltaTime;
};

struct VS_Input {
    uint VertexID   : SV_VertexID;
    uint InstanceID : SV_InstanceID;
};

struct PS_Input {
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
};

static const float2 c_Offsets[4] = {
    float2(-0.5, -0.5), float2(-0.5, 0.5),
    float2(0.5, -0.5), float2(0.5, 0.5)
};

PS_Input VSMain(VS_Input In) {
    PS_Input Out;
    ParticleData p = g_Particles[In.InstanceID];
    
    // View space billboard
    float4 viewPos = mul(float4(p.position, 1.0), ViewMatrix);
    
    // Rotation (degrees to radians)
    float rad = p.rotation * 3.14159265 / 180.0;
    float cosR = cos(rad);
    float sinR = sin(rad);
    
    float2 localOffset = c_Offsets[In.VertexID] * p.size * 10.0; // Base size multiplier
    float2 rotatedOffset;
    rotatedOffset.x = localOffset.x * cosR - localOffset.y * sinR;
    rotatedOffset.y = localOffset.x * sinR + localOffset.y * cosR;
    
    viewPos.xy += rotatedOffset;
    
    Out.Pos = mul(viewPos, ProjMatrix);
    Out.UV = c_Offsets[In.VertexID] + 0.5;
    Out.Color = p.color;
    
    return Out;
}
)";

const char* ParticlePSSource = R"(
struct PS_Input {
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
};

float4 PSMain(PS_Input In) : SV_Target {
    float dist = length(In.UV - 0.5);
    if (dist > 0.5) discard;
    
    // Soft circle
    float alpha = smoothstep(0.5, 0.4, dist);
    return float4(In.Color.rgb, In.Color.a * alpha);
}
)";

ParticleRenderer::ParticleRenderer(GpuContext& context)
    : context_(context), pImpl_(new Impl())
{
}
ParticleRenderer::~ParticleRenderer()
{
    delete pImpl_;
}

void ParticleRenderer::initialize(size_t maxParticles) {
    maxParticles_ = maxParticles;
    createBuffers();
    createPSO();
}

void ParticleRenderer::setFrameCostStats(ArtifactCore::RenderCostStats* stats)
{
    frameCostStats_ = stats;
}

void ParticleRenderer::createBuffers() {
    auto pDevice = context_.RenderDevice();

    // 1. Particle Structured Buffer
    BufferDesc BuffDesc;
    BuffDesc.Name              = "Particle Structured Buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.Size              = sizeof(ParticleVertex) * maxParticles_;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(ParticleVertex);
    pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pParticleBuffer_);

    // 2. Constant Buffer
    BuffDesc.Name              = "Particle Constants CB";
    BuffDesc.Usage             = USAGE_DYNAMIC;
    BuffDesc.Size              = sizeof(ShaderConstants);
    BuffDesc.BindFlags         = BIND_UNIFORM_BUFFER;
    BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
    BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
    BuffDesc.ElementByteStride = 0;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pConstantBuffer_);
}

void ParticleRenderer::createPSO() {
    auto pDevice = context_.RenderDevice();
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    
    PSOCreateInfo.PSODesc.Name = "Particle Rendering PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // Use Triangle Strip for 4 vertices
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = DefaultParticleRTVFormat;
    
    // Alpha blending (Additive by default for many particle effects, or Normal)
    auto& RT0 = PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0.BlendEnable = true;
    RT0.SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend   = BLEND_FACTOR_ONE; // Additive
    RT0.BlendOp     = BLEND_OPERATION_ADD;

    // Compile Shaders (output-param style per new GPUComputeContext API)
    RefCntAutoPtr<IShader> vs, ps;
    context_.CompileShader(ParticleVSSource, SHADER_TYPE_VERTEX, "VSMain", &vs);
    context_.CompileShader(ParticlePSSource, SHADER_TYPE_PIXEL,  "PSMain", &ps);

    PSOCreateInfo.pVS = vs;
    PSOCreateInfo.pPS = ps;

    // Layout
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    static std::array<ShaderResourceVariableDesc, 1> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = (Uint32)Vars.size();

    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pImpl_->pPSO_);

    if (!pImpl_->pPSO_) {
        qWarning("[ParticleRenderer] PSO creation FAILED — "
                 "check shader compilation and RTV format");
        return;
    }
    qDebug() << "[ParticleRenderer] PSO created successfully";

    // Bind Constants cbuffer (static variable — bound once at PSO level)
    auto* pConstVar = pImpl_->pPSO_->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if (!pConstVar) {
        qWarning("[ParticleRenderer] 'Constants' cbuffer not found in PSO "
                 "— static variable name mismatch");
        return;
    }
    pConstVar->Set(pImpl_->pConstantBuffer_);
    pImpl_->pPSO_->CreateShaderResourceBinding(&pImpl_->pSRB_, true);
}

void ParticleRenderer::updateBuffer(const ParticleRenderData& data) {
    if (data.particles.empty()) return;
    auto pContext = context_.DeviceContext();
    
    size_t count = std::min(data.particles.size(), maxParticles_);
    pContext->UpdateBuffer(pImpl_->pParticleBuffer_, 0, sizeof(ParticleVertex) * count, 
                          data.particles.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (frameCostStats_) {
        ++frameCostStats_->bufferUpdates;
    }
}

void ParticleRenderer::prepare(IDeviceContext* pContext) {
    if (!pImpl_->pPSO_ || !pImpl_->pSRB_) {
        qWarning("[ParticleRenderer] prepare() called but PSO/SRB is null — "
                 "particle rendering skipped");
        return;
    }

    // Update Constants
    void* pData = nullptr;
    pContext->MapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if(pData) {
        memcpy(pData, &constants_, sizeof(ShaderConstants));
        pContext->UnmapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE);
        if (frameCostStats_) {
            ++frameCostStats_->bufferUpdates;
        }
    }

    // Set SRV
    pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles")->Set(pImpl_->pParticleBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    pContext->SetPipelineState(pImpl_->pPSO_);
    if (frameCostStats_) {
        ++frameCostStats_->psoSwitches;
        ++frameCostStats_->srbCommits;
    }
    pContext->CommitShaderResources(pImpl_->pSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ParticleRenderer::draw(IDeviceContext* pContext, size_t activeCount) {
    if (activeCount == 0) return;
    
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices  = 4;
    drawAttrs.NumInstances = (Uint32)activeCount;
    // drawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    drawAttrs.Flags        = DRAW_FLAG_NONE;
    if (frameCostStats_) {
        ++frameCostStats_->drawCalls;
    }
    pContext->Draw(drawAttrs);
}

void ParticleRenderer::setProjectionMatrix(const float* matrix) {
    memcpy(constants_.projMatrix, matrix, sizeof(float) * 16);
}

void ParticleRenderer::setViewMatrix(const float* matrix) {
    memcpy(constants_.viewMatrix, matrix, sizeof(float) * 16);
}

} // namespace ArtifactCore

