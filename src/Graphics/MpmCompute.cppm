module;
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

module Graphics.MpmCompute;

namespace ArtifactCore {

namespace {

// Shared declarations + helpers concatenated in front of every kernel body.
// Mirrors MpmSolver2D math: quadratic B-spline weights, fixed corotated PK1
// stress with polar decomposition, APIC transfer. Grid accumulators store
// ordered-int encoded floats so plain uint InterlockedAdd works on SM5.0.
constexpr const char* g_CommonHLSL = R"(
cbuffer MpmParams : register(b0) {
    uint ParticleCount;
    uint NodeCount;
    uint GridNx;
    uint GridNy;
    float CellSize;
    float OriginX;
    float OriginY;
    float InvCellSize;
    float Dt;
    float GravityX;
    float GravityY;
    float Mu;
    float Lambda;
    float Damping;
    float BoundaryFriction;
    float Bxmin;
    float Bxmax;
    float Bymin;
    float Bymax;
    float HasBoundary;
};

struct MpmParticle {
    float2 pos;
    float2 vel;
    float  mass;
    float  volume0;
    float4 F;             // m00, m01, m10, m11
    float4 Fp;
    float4 C;
    float  plasticStrain;
    float  active;
    float2 pad;
};

uint FloatToOrderedInt(float v) {
    uint u = asuint(v);
    return (u & 0x80000000u) ? ~u : (u | 0x80000000u);
}

float OrderedIntToFloat(uint i) {
    uint u = (i & 0x80000000u) ? (i & 0x7FFFFFFFu) : ~i;
    return asfloat(u);
}

// Quadratic B-spline. Mirrors the CPU kernel: the value weight intentionally
// compares the raw world-space offset (same as MpmSolver2D::weight), while
// the gradient applies the 1/cellSize factor like MpmSolver2D::dweight.
float WeightN(float r) {
    float a = abs(r);
    if (a >= 1.5) return 0.0;
    if (a >= 0.5) { float t = 1.5 - a; return 0.5 * t * t; }
    return 0.75 - a * a;
}

float WeightGrad(float r) {
    float a = abs(r);
    if (a >= 1.5) return 0.0;
    float sgn = (r > 0.0) ? 1.0 : ((r < 0.0) ? -1.0 : 0.0);
    if (a >= 0.5) return -(1.5 - a) * sgn / CellSize;
    return -2.0 * a * sgn / CellSize;
}

float2 MatVec(float4 m, float2 v) {
    return float2(m.x * v.x + m.y * v.y, m.z * v.x + m.w * v.y);
}

float4 MatMulMM(float4 a, float4 b) {
    return float4(a.x * b.x + a.y * b.z,
                  a.x * b.y + a.y * b.w,
                  a.z * b.x + a.w * b.z,
                  a.z * b.y + a.w * b.w);
}

float4 MatInverse(float4 m) {
    float d = m.x * m.w - m.y * m.z;
    if (abs(d) < 1e-20) return float4(1, 0, 0, 1);
    float inv = 1.0 / d;
    return float4(m.w * inv, -m.y * inv, -m.z * inv, m.x * inv);
}

float4 PolarDecomposition(float4 F) {
    float4 R = F;
    [loop]
    for (int it = 0; it < 20; ++it) {
        float det = R.x * R.w - R.y * R.z;
        if (abs(det) < 1e-20) break;
        float invDet = 1.0 / det;
        float4 RTinv = float4(R.w * invDet, -R.z * invDet,
                              -R.y * invDet,  R.x * invDet);
        float4 Rn = 0.5 * (R + RTinv);
        float diff = abs(Rn.x - R.x) + abs(Rn.y - R.y) +
                     abs(Rn.z - R.z) + abs(Rn.w - R.w);
        R = Rn;
        if (diff < 1e-8) break;
    }
    return R;
}

float4 FirstPiolaKirchhoff(float4 Fe) {
    float J = Fe.x * Fe.w - Fe.y * Fe.z;
    float4 R = PolarDecomposition(Fe);
    float4 FinvT = MatInverse(Fe);
    FinvT = float4(FinvT.x, Finv.z, Finv.y, Finv.w);
    float s = Lambda * J * (J - 1.0);
    float twoMu = 2.0 * Mu;
    return float4(twoMu * (Fe.x - R.x) + s * FinvT.x,
                  twoMu * (Fe.y - R.y) + s * FinvT.y,
                  twoMu * (Fe.z - R.z) + s * FinvT.z,
                  twoMu * (Fe.w - R.w) + s * FinvT.w);
}
)";

constexpr const char* g_ClearBody = R"(
RWStructuredBuffer<uint>  g_GridMass     : register(u0);
RWStructuredBuffer<uint2> g_GridMomentum : register(u1);
RWStructuredBuffer<uint2> g_GridForce    : register(u2);

[numthreads(64, 1, 1)]
void MpmClearMain(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= NodeCount) return;
    g_GridMass[idx] = FloatToOrderedInt(0.0);
    g_GridMomentum[idx] = uint2(FloatToOrderedInt(0.0), FloatToOrderedInt(0.0));
    g_GridForce[idx] = uint2(FloatToOrderedInt(0.0), FloatToOrderedInt(0.0));
}
)";

constexpr const char* g_P2GBody = R"(
StructuredBuffer<MpmParticle> g_Particles : register(t0);
RWStructuredBuffer<uint>  g_GridMass     : register(u0);
RWStructuredBuffer<uint2> g_GridMomentum : register(u1);

[numthreads(64, 1, 1)]
void MpmP2GMain(uint3 DTid : SV_DispatchThreadID) {
    uint pid = DTid.x;
    if (pid >= ParticleCount) return;
    MpmParticle p = g_Particles[pid];
    if (p.active < 0.5) return;

    float fx = (p.pos.x - OriginX) * InvCellSize;
    float fy = (p.pos.y - OriginY) * InvCellSize;
    int ix = (int)floor(fx);
    int iy = (int)floor(fy);

    [unroll]
    for (int dj = -1; dj <= 2; ++dj) {
        [unroll]
        for (int di = -1; di <= 2; ++di) {
            int ni = ix + di;
            int nj = iy + dj;
            if (ni < 0 || ni >= (int)GridNx || nj < 0 || nj >= (int)GridNy) continue;
            float2 nodePos = float2(OriginX + ((float)ni + 0.5) * CellSize,
                                    OriginY + ((float)nj + 0.5) * CellSize);
            float rx = p.pos.x - nodePos.x;
            float ry = p.pos.y - nodePos.y;
            float w = WeightN(rx) * WeightN(ry);
            if (w <= 0.0) continue;
            float2 diff = nodePos - p.pos;
            float apicX = p.C.x * diff.x + p.C.y * diff.y;
            float apicY = p.C.z * diff.x + p.C.w * diff.y;
            uint idx = ni + nj * GridNx;
            InterlockedAdd(g_GridMass[idx], FloatToOrderedInt(w * p.mass));
            InterlockedAdd(g_GridMomentum[idx].x,
                FloatToOrderedInt(w * (p.mass * p.vel.x + apicX)));
            InterlockedAdd(g_GridMomentum[idx].y,
                FloatToOrderedInt(w * (p.mass * p.vel.y + apicY)));
        }
    }
}
)";

constexpr const char* g_ForceBody = R"(
StructuredBuffer<MpmParticle> g_Particles : register(t0);
RWStructuredBuffer<uint2> g_GridForce : register(u0);

[numthreads(64, 1, 1)]
void MpmForcesMain(uint3 DTid : SV_DispatchThreadID) {
    uint pid = DTid.x;
    if (pid >= ParticleCount) return;
    MpmParticle p = g_Particles[pid];
    if (p.active < 0.5 || p.volume0 <= 0.0) return;

    float4 Fe = MatMulMM(p.F, MatInverse(p.Fp));
    float4 P = FirstPiolaKirchhoff(Fe);

    float fx = (p.pos.x - OriginX) * InvCellSize;
    float fy = (p.pos.y - OriginY) * InvCellSize;
    int ix = (int)floor(fx);
    int iy = (int)floor(fy);

    [unroll]
    for (int dj = -1; dj <= 2; ++dj) {
        [unroll]
        for (int di = -1; di <= 2; ++di) {
            int ni = ix + di;
            int nj = iy + dj;
            if (ni < 0 || ni >= (int)GridNx || nj < 0 || nj >= (int)GridNy) continue;
            float2 nodePos = float2(OriginX + ((float)ni + 0.5) * CellSize,
                                    OriginY + ((float)nj + 0.5) * CellSize);
            float rx = p.pos.x - nodePos.x;
            float ry = p.pos.y - nodePos.y;
            float gradW = WeightGrad(rx) * WeightN(ry);
            float gradH = WeightN(rx) * WeightGrad(ry);
            uint idx = ni + nj * GridNx;
            // f_i -= V_p * P * grad(w_i)
            InterlockedAdd(g_GridForce[idx].x,
                FloatToOrderedInt(-p.volume0 * (P.x * gradW + P.y * gradH)));
            InterlockedAdd(g_GridForce[idx].y,
                FloatToOrderedInt(-p.volume0 * (P.z * gradW + P.w * gradH)));
        }
    }
}
)";

constexpr const char* g_GridUpdateBody = R"(
RWStructuredBuffer<uint>  g_GridMass     : register(u0);
RWStructuredBuffer<uint2> g_GridMomentum : register(u1);
RWStructuredBuffer<uint2> g_GridForce    : register(u2);

[numthreads(64, 1, 1)]
void MpmGridUpdateMain(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= NodeCount) return;
    float m = OrderedIntToFloat(g_GridMass[idx]);
    if (m <= 0.0) return;

    float momX = OrderedIntToFloat(g_GridMomentum[idx].x);
    float momY = OrderedIntToFloat(g_GridMomentum[idx].y);
    float fX = OrderedIntToFloat(g_GridForce[idx].x);
    float fY = OrderedIntToFloat(g_GridForce[idx].y);
    float invM = 1.0 / m;

    // v = (mv + dt * (gravity + f)) / m, then optional global damping.
    float vx = momX * invM + Dt * (GravityX + fX * invM);
    float vy = momY * invM + Dt * (GravityY + fY * invM);
    if (Damping > 0.0) {
        vx *= (1.0 - Damping);
        vy *= (1.0 - Damping);
    }

    // Boundary velocity conditions (post-update pass of the CPU lane).
    if (HasBoundary > 0.5) {
        uint i = idx % GridNx;
        uint j = idx / GridNx;
        float halfCell = 0.5 * CellSize;
        float posX = OriginX + ((float)i + 0.5) * CellSize;
        float posY = OriginY + ((float)j + 0.5) * CellSize;
        if (posX - halfCell <= Bxmin) {
            vx = max(0.0, vx + BoundaryFriction * vx);
            vy *= (1.0 - BoundaryFriction);
        }
        if (posX + halfCell >= Bxmax) {
            vx = min(0.0, vx - BoundaryFriction * vx);
            vy *= (1.0 - BoundaryFriction);
        }
        if (posY - halfCell <= Bymin) {
            vy = max(0.0, vy + BoundaryFriction * vy);
            vx *= (1.0 - BoundaryFriction);
        }
        if (posY + halfCell >= Bymax) {
            vy = min(0.0, vy - BoundaryFriction * vy);
            vx *= (1.0 - BoundaryFriction);
        }
    }

    g_GridMomentum[idx] = uint2(FloatToOrderedInt(vx * m),
                                FloatToOrderedInt(vy * m));
}
)";

constexpr const char* g_G2PBody = R"(
RWStructuredBuffer<MpmParticle> g_Particles    : register(u0);
StructuredBuffer<uint>          g_GridMass     : register(t1);
StructuredBuffer<uint2>         g_GridMomentum : register(t2);

[numthreads(64, 1, 1)]
void MpmG2PMain(uint3 DTid : SV_DispatchThreadID) {
    uint pid = DTid.x;
    if (pid >= ParticleCount) return;
    MpmParticle p = g_Particles[pid];
    if (p.active < 0.5) return;

    float fx = (p.pos.x - OriginX) * InvCellSize;
    float fy = (p.pos.y - OriginY) * InvCellSize;
    int ix = (int)floor(fx);
    int iy = (int)floor(fy);

    float2 newVel = float2(0.0, 0.0);
    float4 newC = float4(0, 0, 0, 0);
    float4 gradV = float4(0, 0, 0, 0);
    float dScale = 4.0 / (CellSize * CellSize);

    [unroll]
    for (int dj = -1; dj <= 2; ++dj) {
        [unroll]
        for (int di = -1; di <= 2; ++di) {
            int ni = ix + di;
            int nj = iy + dj;
            if (ni < 0 || ni >= (int)GridNx || nj < 0 || nj >= (int)GridNy) continue;
            float2 nodePos = float2(OriginX + ((float)ni + 0.5) * CellSize,
                                    OriginY + ((float)nj + 0.5) * CellSize);
            float rx = p.pos.x - nodePos.x;
            float ry = p.pos.y - nodePos.y;
            float wx = WeightN(rx);
            float wy = WeightN(ry);
            float w = wx * wy;
            if (w <= 0.0) continue;

            uint idx = ni + nj * GridNx;
            float mass = OrderedIntToFloat(g_GridMass[idx]);
            if (mass <= 0.0) continue;
            float2 vNode = float2(OrderedIntToFloat(g_GridMomentum[idx].x),
                                  OrderedIntToFloat(g_GridMomentum[idx].y)) / mass;

            newVel += w * vNode;
            float2 diff = nodePos - p.pos;
            newC.x += w * vNode.x * diff.x * dScale;
            newC.y += w * vNode.x * diff.y * dScale;
            newC.z += w * vNode.y * diff.x * dScale;
            newC.w += w * vNode.y * diff.y * dScale;

            float gradW = WeightGrad(rx) * wy;
            float gradH = wx * WeightGrad(ry);
            gradV.x += vNode.x * gradW;
            gradV.y += vNode.x * gradH;
            gradV.z += vNode.y * gradW;
            gradV.w += vNode.y * gradH;
        }
    }

    p.vel = newVel;
    p.C = newC;
    p.pos += newVel * Dt;

    // F_new = (I + dt * gradV) * F_old
    float4 dF = float4(1.0 + Dt * gradV.x, Dt * gradV.y,
                       Dt * gradV.z, 1.0 + Dt * gradV.w);
    p.F = MatMulMM(dF, p.F);

    g_Particles[pid] = p;
}
)";

const std::string kClearSource = std::string(g_CommonHLSL) + g_ClearBody;
const std::string kP2GSource = std::string(g_CommonHLSL) + g_P2GBody;
const std::string kForceSource = std::string(g_CommonHLSL) + g_ForceBody;
const std::string kGridUpdateSource = std::string(g_CommonHLSL) + g_GridUpdateBody;
const std::string kG2PSource = std::string(g_CommonHLSL) + g_G2PBody;

} // namespace

// ============================================================================
// C++ Implementation
// ============================================================================

MpmGPUCompute::MpmGPUCompute(GpuContext& context)
    : context_(context), clearExec_(context), p2gExec_(context),
      forceExec_(context), gridUpdateExec_(context), g2pExec_(context) {}

MpmGPUCompute::~MpmGPUCompute() = default;

bool MpmGPUCompute::initialize(std::uint32_t gridNx, std::uint32_t gridNy,
                               std::uint32_t maxParticles) {
    nx_ = gridNx;
    ny_ = gridNy;
    capacity_ = maxParticles;
    ready_ = false;

    if (!createBuffers()) return false;
    if (!buildPipelines()) return false;

    ready_ = true;
    return true;
}

bool MpmGPUCompute::createBuffers() {
    auto* device = context_.RenderDevice();
    if (!device || nx_ == 0 || ny_ == 0 || capacity_ == 0) return false;

    const std::uint32_t nodeCount = nx_ * ny_;

    BufferDesc particleDesc;
    particleDesc.Name = "MPM Particles";
    particleDesc.Size = static_cast<Uint64>(capacity_) *
                        sizeof(MpmGpuParticle);
    particleDesc.Usage = USAGE_DEFAULT;
    particleDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    particleDesc.Mode = BUFFER_MODE_STRUCTURED;
    particleDesc.ElementByteStride = sizeof(MpmGpuParticle);
    device->CreateBuffer(particleDesc, nullptr, &pParticleBuffer_);
    if (!pParticleBuffer_) return false;

    BufferDesc massDesc;
    massDesc.Name = "MPM Grid Mass";
    massDesc.Size = static_cast<Uint64>(nodeCount) * sizeof(Uint32);
    massDesc.Usage = USAGE_DEFAULT;
    massDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    massDesc.Mode = BUFFER_MODE_STRUCTURED;
    massDesc.ElementByteStride = sizeof(Uint32);
    device->CreateBuffer(massDesc, nullptr, &pGridMass_);

    BufferDesc momentumDesc = massDesc;
    momentumDesc.Name = "MPM Grid Momentum";
    momentumDesc.ElementByteStride = sizeof(Uint32) * 2;
    momentumDesc.Size = static_cast<Uint64>(nodeCount) * sizeof(Uint32) * 2;
    device->CreateBuffer(momentumDesc, nullptr, &pGridMomentum_);

    BufferDesc forceDesc = momentumDesc;
    forceDesc.Name = "MPM Grid Force";
    device->CreateBuffer(forceDesc, nullptr, &pGridForce_);

    BufferDesc cbDesc;
    cbDesc.Name = "MPM Params CB";
    cbDesc.Size = sizeof(MpmGpuParams);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(cbDesc, nullptr, &pConstantBuffer_);

    return pGridMass_ && pGridMomentum_ && pGridForce_ && pConstantBuffer_;
}

bool MpmGPUCompute::buildPipelines() {
    {
        static const ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_COMPUTE, "g_GridMass", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridMomentum", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridForce", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "MpmParams", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "MPM Clear";
        desc.shaderSource = kClearSource.c_str();
        desc.entryPoint = "MpmClearMain";
        desc.variables = vars;
        desc.variableCount = 4;
        if (!clearExec_.build(desc)) return false;
        if (!clearExec_.createShaderResourceBinding(true)) return false;
        clearExec_.setBuffer("MpmParams", pConstantBuffer_);
        clearExec_.setBuffer("g_GridMass", pGridMass_);
        clearExec_.setBuffer("g_GridMomentum", pGridMomentum_);
        clearExec_.setBuffer("g_GridForce", pGridForce_);
    }
    {
        static const ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_COMPUTE, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridMass", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridMomentum", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "MpmParams", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "MPM P2G";
        desc.shaderSource = kP2GSource.c_str();
        desc.entryPoint = "MpmP2GMain";
        desc.variables = vars;
        desc.variableCount = 4;
        if (!p2gExec_.build(desc)) return false;
        if (!p2gExec_.createShaderResourceBinding(true)) return false;
        p2gExec_.setBuffer("MpmParams", pConstantBuffer_);
        p2gExec_.setBuffer("g_Particles", pParticleBuffer_);
        p2gExec_.setBuffer("g_GridMass", pGridMass_);
        p2gExec_.setBuffer("g_GridMomentum", pGridMomentum_);
    }
    {
        static const ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_COMPUTE, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridForce", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "MpmParams", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "MPM Forces";
        desc.shaderSource = kForceSource.c_str();
        desc.entryPoint = "MpmForcesMain";
        desc.variables = vars;
        desc.variableCount = 3;
        if (!forceExec_.build(desc)) return false;
        if (!forceExec_.createShaderResourceBinding(true)) return false;
        forceExec_.setBuffer("MpmParams", pConstantBuffer_);
        forceExec_.setBuffer("g_Particles", pParticleBuffer_);
        forceExec_.setBuffer("g_GridForce", pGridForce_);
    }
    {
        static const ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_COMPUTE, "g_GridMass", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridMomentum", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridForce", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "MpmParams", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "MPM GridUpdate";
        desc.shaderSource = kGridUpdateSource.c_str();
        desc.entryPoint = "MpmGridUpdateMain";
        desc.variables = vars;
        desc.variableCount = 4;
        if (!gridUpdateExec_.build(desc)) return false;
        if (!gridUpdateExec_.createShaderResourceBinding(true)) return false;
        gridUpdateExec_.setBuffer("MpmParams", pConstantBuffer_);
        gridUpdateExec_.setBuffer("g_GridMass", pGridMass_);
        gridUpdateExec_.setBuffer("g_GridMomentum", pGridMomentum_);
        gridUpdateExec_.setBuffer("g_GridForce", pGridForce_);
    }
    {
        static const ShaderResourceVariableDesc vars[] = {
            { SHADER_TYPE_COMPUTE, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridMass", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "g_GridMomentum", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
            { SHADER_TYPE_COMPUTE, "MpmParams", SHADER_RESOURCE_VARIABLE_TYPE_STATIC },
        };
        ComputePipelineDesc desc;
        desc.name = "MPM G2P";
        desc.shaderSource = kG2PSource.c_str();
        desc.entryPoint = "MpmG2PMain";
        desc.variables = vars;
        desc.variableCount = 4;
        if (!g2pExec_.build(desc)) return false;
        if (!g2pExec_.createShaderResourceBinding(true)) return false;
        g2pExec_.setBuffer("MpmParams", pConstantBuffer_);
        g2pExec_.setBuffer("g_Particles", pParticleBuffer_);
        g2pExec_.setBuffer("g_GridMass", pGridMass_);
        g2pExec_.setBuffer("g_GridMomentum", pGridMomentum_);
    }
    return true;
}

void MpmGPUCompute::uploadParticles(IDeviceContext* pContext,
                                    const MpmGpuParticle* particles,
                                    std::uint32_t count) {
    if (!ready_ || !pContext || !particles) return;
    const std::uint32_t clamped = std::min(count, capacity_);
    if (clamped == 0) return;
    particleCount_ = clamped;
    pContext->UpdateBuffer(pParticleBuffer_, 0,
                           static_cast<Uint64>(clamped) * sizeof(MpmGpuParticle),
                           particles, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void MpmGPUCompute::simulateSubsteps(IDeviceContext* pContext, int substeps,
                                     const MpmGpuParams& params) {
    if (!ready_ || !pContext || substeps <= 0 || params.particleCount == 0) return;
    if (params.nodeCount != nx_ * ny_) return;

    {
        void* pData = nullptr;
        pContext->MapBuffer(pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
        if (pData) {
            std::memcpy(pData, &params, sizeof(params));
            pContext->UnmapBuffer(pConstantBuffer_, MAP_WRITE);
        }
    }

    const auto particleAttribs = ComputeExecutor::makeDispatchAttribs(
        params.particleCount, 1, 1, 64);
    const auto nodeAttribs = ComputeExecutor::makeDispatchAttribs(
        params.nodeCount, 1, 1, 64);

    for (int step = 0; step < substeps; ++step) {
        clearExec_.dispatch(pContext, nodeAttribs,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        p2gExec_.dispatch(pContext, particleAttribs,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        forceExec_.dispatch(pContext, particleAttribs,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        gridUpdateExec_.dispatch(pContext, nodeAttribs,
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        g2pExec_.dispatch(pContext, particleAttribs,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}

void MpmGPUCompute::readbackParticles(IDeviceContext* pContext,
                                      MpmGpuParticle* outParticles,
                                      std::uint32_t count) {
    if (!ready_ || !pContext || !outParticles) return;
    const std::uint32_t clamped = std::min(count, capacity_);
    if (clamped == 0) return;

    auto* device = context_.RenderDevice();
    if (!device) return;

    BufferDesc stagingDesc;
    stagingDesc.Name = "MPM Particle Readback";
    stagingDesc.Size = static_cast<Uint64>(capacity_) * sizeof(MpmGpuParticle);
    stagingDesc.Usage = USAGE_STAGING;
    stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
    stagingDesc.Mode = BUFFER_MODE_STRUCTURED;
    stagingDesc.ElementByteStride = sizeof(MpmGpuParticle);

    RefCntAutoPtr<IBuffer> staging;
    device->CreateBuffer(stagingDesc, nullptr, &staging);
    if (!staging) return;

    CopyBufferAttribs copy(pParticleBuffer_,
                           RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                           staging, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->CopyBuffer(copy);
    pContext->Flush();
    pContext->WaitForIdle();

    void* mapped = nullptr;
    if (pContext->MapBuffer(staging, MAP_READ, MAP_FLAG_NONE, mapped)) {
        std::memcpy(outParticles, mapped,
                    static_cast<std::size_t>(clamped) * sizeof(MpmGpuParticle));
        pContext->UnmapBuffer(staging, MAP_READ);
    }
}

} // namespace ArtifactCore
