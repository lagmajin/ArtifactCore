module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>

module Graphics.ParticleRenderer;

namespace ArtifactCore {

const char* ParticleVSSource = R"(
struct ParticleData {
    uint2 id; // uint64_t
    uint  seed;
    uint  flags;
    float age;
    float lifetime;
    float3 position;
    float3 prevPosition;
    float3 velocity;
    float3 acceleration;
    float3 rotation;
    float3 angularVelocity;
    float2 scale;
    float size;
    float mass;
    float drag;
    float opacity;
    float4 color;
    float4 custom0;
    float4 custom1;
    int textureIndex;
    int blendMode;
    float lastSubEmitAge;
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
    float  Opacity : OPACITY;
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
    float2 offset = c_Offsets[In.VertexID] * p.size * p.scale;
    viewPos.xy += offset;
    
    Out.Pos = mul(viewPos, ProjMatrix);
    Out.UV = c_Offsets[In.VertexID] + 0.5;
    Out.Color = p.color;
    Out.Opacity = p.opacity;
    
    return Out;
}
)";

const char* ParticlePSSource = R"(
struct PS_Input {
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
    float  Opacity : OPACITY;
};

float4 PSMain(PS_Input In) : SV_Target {
    float dist = length(In.UV - 0.5);
    if (dist > 0.5) discard;
    
    // Soft circle
    float alpha = smoothstep(0.5, 0.4, dist);
    return float4(In.Color.rgb, In.Color.a * In.Opacity * alpha);
}
)";

ParticleRenderer::ParticleRenderer(GpuContext& context) : context_(context) {}
ParticleRenderer::~ParticleRenderer() = default;

void ParticleRenderer::initialize(size_t maxParticles) {
    maxParticles_ = maxParticles;
    createBuffers();
    createPSO();
}

void ParticleRenderer::createBuffers() {
    auto pDevice = context_.D3D12RenderDevice();

    // 1. Particle Structured Buffer
    BufferDesc BuffDesc;
    BuffDesc.Name              = "Particle Structured Buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.Size              = sizeof(Particle) * maxParticles_;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
    BuffDesc.ElementByteSize   = sizeof(Particle);
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pParticleBuffer_);

    // 2. Constant Buffer
    BuffDesc.Name              = "Particle Constants CB";
    BuffDesc.Usage             = USAGE_DYNAMIC;
    BuffDesc.Size              = sizeof(ShaderConstants);
    BuffDesc.BindFlags         = BIND_CONSTANT_BUFFER;
    BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
    BuffDesc.ElementByteSize   = 0;
    BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pConstantBuffer_);
}

void ParticleRenderer::createPSO() {
    auto pDevice = context_.D3D12RenderDevice();
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    
    PSOCreateInfo.PSODesc.Name = "Particle Rendering PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // Use Triangle Strip for 4 vertices
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM; // Match current UI format
    
    // Alpha blending (Normal)
    auto& RT0 = PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0.BlendEnable = true;
    RT0.SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOp     = BLEND_OPERATION_ADD;

    // Compile Shaders
    auto vs = context_.CompileShader(ParticleVSSource, SHADER_TYPE_VERTEX, "VSMain");
    auto ps = context_.CompileShader(ParticlePSSource, SHADER_TYPE_PIXEL, "PSMain");

    PSOCreateInfo.pVS = vs;
    PSOCreateInfo.pPS = ps;

    // Layout
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    static std::array<ShaderResourceVariableDesc, 1> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = (Uint32)Vars.size();

    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO_);

    // Bind Constants
    pPSO_->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(pConstantBuffer_);
    pPSO_->CreateShaderResourceBinding(&pSRB_, true);
}

void ParticleRenderer::updateBuffer(const std::vector<Particle>& particles, size_t activeCount) {
    if (activeCount == 0) return;
    auto pContext = context_.D3D12DeviceContext();
    
    // Upload current particles directly to GPU buffer
    pContext->UpdateBuffer(pParticleBuffer_, 0, sizeof(Particle) * activeCount, particles.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ParticleRenderer::prepare(IDeviceContext* pContext) {
    // Update Constants
    void* pData = nullptr;
    pContext->MapBuffer(pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if(pData) {
        memcpy(pData, &constants_, sizeof(ShaderConstants));
        pContext->UnmapBuffer(pConstantBuffer_, MAP_WRITE);
    }

    // Set SRV
    pSRB_->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles")->Set(pParticleBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    pContext->SetPipelineState(pPSO_);
    pContext->CommitShaderResources(pSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ParticleRenderer::draw(IDeviceContext* pContext, size_t activeCount) {
    if (activeCount == 0) return;
    
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices  = 4;            // 4 vertices (Strip) -> 1 Box
    drawAttrs.NumInstances = (Uint32)activeCount;
    drawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    
    pContext->Draw(drawAttrs);
}

void ParticleRenderer::setProjectionMatrix(const float* matrix) {
    memcpy(constants_.projMatrix, matrix, sizeof(float) * 16);
}

void ParticleRenderer::setViewMatrix(const float* matrix) {
    memcpy(constants_.viewMatrix, matrix, sizeof(float) * 16);
}

} // namespace ArtifactCore
