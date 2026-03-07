module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>

module Graphics.ParticleCompute;

namespace ArtifactCore {

const char* ParticleUpdateCSSource = R"(
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

RWStructuredBuffer<ParticleData> g_ParticleBuffer : register(u0);

cbuffer Constants : register(b0) {
    float DeltaTime;
    float Time;
    uint  MaxParticles;
    float NoiseStrength;
    float AudioIntensity;
};

StructuredBuffer<float> g_AudioSpectrum : register(t1);

// 簡易的なノイズ関数 (後で本格的なものへ拡張可能)
float3 hash33(float3 p) {
    p = frac(p * float3(.1031, .1030, .0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.xxy + p.yxx) * p.zyx);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= MaxParticles) return;

    // 生きているパーティクルのみ更新 (簡略化：寿命チェック)
    if (g_ParticleBuffer[idx].age >= g_ParticleBuffer[idx].lifetime) return;

    ParticleData p = g_ParticleBuffer[idx];
    
    // 1. 位置の保存 (Motion Blur 用)
    p.prevPosition = p.position;

    // 2. 加速度の適用
    p.velocity += p.acceleration * DeltaTime;
    
    // 3. 速度に応じたドラッグ (抵抗)
    p.velocity *= (1.0 - p.drag * DeltaTime);

    // 4. ノイズ (乱流) の適用
    float3 n = (hash33(p.position * 0.1 + Time) - 0.5) * NoiseStrength;
    p.velocity += n * DeltaTime;

    // 4.5. Audio Reactivity (音への反応)
    uint binIdx = (idx % 256); // 簡易的にインデックスでサンプリング
    float freqVal = g_AudioSpectrum[binIdx];
    p.velocity += p.velocity * freqVal * AudioIntensity * DeltaTime;
    p.size *= (1.0 + freqVal * AudioIntensity * 0.01);
    p.color.rgb += float3(freqVal, 0, 1.0 - freqVal) * AudioIntensity * 0.5;

    // 5. 位置の更新
    p.position += p.velocity * DeltaTime;

    // 6. 加齢
    p.age += DeltaTime;

    // バッファへ書き戻し
    g_ParticleBuffer[idx] = p;
}
)";

ParticleCompute::ParticleCompute(GpuContext& context) : context_(context) {}
ParticleCompute::~ParticleCompute() = default;

void ParticleCompute::initialize(size_t maxParticles) {
    maxParticles_ = maxParticles;
    createBuffers();
    createPSO();
}

void ParticleCompute::createBuffers() {
    auto pDevice = context_.D3D12RenderDevice();

    // 1. RWStructuredBuffer (UAV)
    BufferDesc BuffDesc;
    BuffDesc.Name              = "Particle Compute buffer (UAV)";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.Size              = sizeof(Particle) * maxParticles_;
    // UAV (更新用) と SRV (描画用) の両方として使えるように
    BuffDesc.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    // BuffDesc.ElementByteSize   = sizeof(Particle); // Note: API changed in Diligent
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pParticleBuffer_);

    // 2. Constant Buffer (Simulation Params)
    BuffDesc.Name              = "Particle Compute Constants CB";
    BuffDesc.Usage             = USAGE_DYNAMIC;
    BuffDesc.Size              = sizeof(SimulationConstants);
    // BuffDesc.ElementByteSize   = 0; // Note: API changed in Diligent
    BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pConstantBuffer_);

    // 3. Audio Spectrum Buffer (SRV)
    BuffDesc.Name              = "Audio Spectrum Buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.Size              = sizeof(float) * 256; // 256 bins
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
    // BuffDesc.ElementByteSize   = sizeof(float); // Note: API changed in Diligent
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pAudioSpectrumBuffer_);
}

void ParticleCompute::createPSO() {
    auto pDevice = context_.D3D12RenderDevice();
    
    ComputePipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "Particle Compute PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

    // Compile Compute Shader
    auto cs = context_.CompileShader(ParticleUpdateCSSource, SHADER_TYPE_COMPUTE, "CSMain");
    PSOCreateInfo.pCS = cs;

    // Resource Layout (UAV と Constants)
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    static ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_COMPUTE, "g_ParticleBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_AudioSpectrum", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = 2;

    pDevice->CreateComputePipelineState(PSOCreateInfo, &pUpdatePSO_);

    // Bind Constants (Static)
    pUpdatePSO_->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(pConstantBuffer_);
    
    pUpdatePSO_->CreateShaderResourceBinding(&pUpdateSRB_, true);
}

void ParticleCompute::dispatch(IDeviceContext* pContext, float dt) {
    // 1. Constants 更新
    constants_.deltaTime = dt;
    constants_.time += dt;
    constants_.maxParticles = (uint32_t)maxParticles_;
    constants_.noiseStrength = 2.0f; // Default

    void* pData = nullptr;
    pContext->MapBuffer(pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if(pData) {
        memcpy(pData, &constants_, sizeof(SimulationConstants));
        pContext->UnmapBuffer(pConstantBuffer_, MAP_WRITE);
    }

    // 2. UAV & SRV セット
    pUpdateSRB_->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleBuffer")->Set(pParticleBuffer_->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    pUpdateSRB_->GetVariableByName(SHADER_TYPE_COMPUTE, "g_AudioSpectrum")->Set(pAudioSpectrumBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    // 3. Dispatch
    pContext->SetPipelineState(pUpdatePSO_);
    pContext->CommitShaderResources(pUpdateSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    DispatchComputeAttribs Attribs;
    Attribs.ThreadGroupCountX = ((uint32_t)maxParticles_ + 63) / 64;
    pContext->DispatchCompute(Attribs);
}

void ParticleCompute::uploadParticles(const std::vector<Particle>& particles, size_t count) {
    if (count == 0) return;
    auto pContext = context_.D3D12DeviceContext();
    pContext->UpdateBuffer(pParticleBuffer_, 0, sizeof(Particle) * std::min(count, maxParticles_), particles.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ParticleCompute::setAudioData(const std::vector<float>& spectrum) {
    if (spectrum.empty()) return;
    auto pContext = context_.D3D12DeviceContext();
    size_t size = std::min(spectrum.size(), (size_t)256) * sizeof(float);
    pContext->UpdateBuffer(pAudioSpectrumBuffer_, 0, size, spectrum.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Intensity は全体の平均とかでとりあえず
    float avg = 0;
    for(size_t i=0; i<std::min(spectrum.size(), (size_t)16); ++i) avg += spectrum[i];
    constants_.audioIntensity = (avg / 16.0f) * 5.0f;
}

} // namespace ArtifactCore
