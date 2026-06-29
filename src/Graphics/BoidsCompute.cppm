module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>
#include <QDebug>
#include <cstring>

module Graphics.BoidsCompute;

namespace ArtifactCore {

const char* BoidsUpdateCSSource = R"(
struct BoidData {
    float4 posType;      // x,y,z,type
    float4 velSpeed;     // x,y,z,maxSpeed
    float4 params;       // maxForce,neighborDist,desiredSep,size
    float4 color;        // r,g,b,unused
};

struct ObstacleData {
    float3 center;
    float radius;
};

RWStructuredBuffer<BoidData> g_Boids : register(u0);
StructuredBuffer<ObstacleData> g_Obstacles : register(t0);

cbuffer Constants : register(b0) {
    float SeparationWeight;
    float AlignmentWeight;
    float CohesionWeight;
    float TargetWeight;
    float WanderWeight;
    float ObstacleWeight;
    float PredatorWeight;
    float PreyWeight;
    float DeltaTime;
    float WanderJitter;
    float WanderRadius;
    float WanderDistance;
    float PredatorMaxSpeed;
    float PredatorMaxForce;
    float PredatorChaseRange;
    float PreyMaxSpeed;
    float PreyMaxForce;
    float PreyFleeRange;
    float TargetX, TargetY, TargetZ;
    uint HasTarget;
    float BoundsX, BoundsY, BoundsZ;
    uint AgentCount;
    uint ObstacleCount;
};

float3 limit(float3 v, float maxVal) {
    float magSq = dot(v, v);
    if (magSq > maxVal * maxVal) {
        float mag = sqrt(magSq);
        return v / mag * maxVal;
    }
    return v;
}

float3 steer(float3 desired, float3 velocity, float maxSpeed, float maxForce) {
    float mag = length(desired);
    if (mag > 0.0) {
        desired = desired / mag * maxSpeed - velocity;
        return limit(desired, maxForce);
    }
    return float3(0, 0, 0);
}

float3 seek(float3 pos, float3 velocity, float3 target, float maxSpeed, float maxForce) {
    float3 desired = target - pos;
    return steer(desired, velocity, maxSpeed, maxForce);
}

float3 avoidBounds(float3 pos, float3 velocity, float3 bounds, float maxSpeed) {
    float3 force = 0;
    float margin = 50.0;
    if (pos.x < -bounds.x/2 + margin) force.x = maxSpeed;
    else if (pos.x > bounds.x/2 - margin) force.x = -maxSpeed;
    if (pos.y < -bounds.y/2 + margin) force.y = maxSpeed;
    else if (pos.y > bounds.y/2 - margin) force.y = -maxSpeed;
    if (pos.z < -bounds.z/2 + margin) force.z = maxSpeed;
    else if (pos.z > bounds.z/2 - margin) force.z = -maxSpeed;
    return force;
}

float3 avoidObstacles(float3 pos, float3 velocity, float maxSpeed, float neighborDistance) {
    float3 force = 0;
    for (uint k = 0; k < ObstacleCount; ++k) {
        float3 diff = pos - g_Obstacles[k].center;
        float dist = length(diff);
        float threatRange = g_Obstacles[k].radius + neighborDistance * 0.5;
        if (dist < threatRange && dist > 0.001) {
            float strength = (threatRange - dist) / threatRange;
            force += normalize(diff) * strength * maxSpeed;
        }
    }
    return force;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
    if (idx >= AgentCount) return;

    BoidData boid = g_Boids[idx];

    // Unpack fields
    float3 pos = boid.posType.xyz;
    uint type = asuint(boid.posType.w);
    float3 vel = boid.velSpeed.xyz;
    float maxSpeed = boid.velSpeed.w;
    float maxForce = boid.params.x;
    float neighborDist = boid.params.y;
    float desiredSep = boid.params.z;
    float size = boid.params.w;
    float3 col = boid.color.xyz;

    float3 sepForce = 0, aliForce = 0, cohForce = 0;
    int sepCount = 0, neighborCount = 0;
    float3 predForce = 0, preyForce = 0;
    int predCount = 0, preyCount = 0;

    float effSepWeight = SeparationWeight;

    // Brute-force O(N^2) neighbor search
    for (uint j = 0; j < AgentCount; ++j) {
        if (j == idx) continue;
        BoidData other = g_Boids[j];
        uint otherType = asuint(other.posType.w);
        float3 otherPos = other.posType.xyz;
        float3 otherVel = other.velSpeed.xyz;

        float3 diff = pos - otherPos;
        float d = length(diff);
        if (d < 0.001) continue;

        float effectiveRange = neighborDist;
        if (type == 1 && otherType == 2) effectiveRange = PredatorChaseRange;
        else if (type == 2 && otherType == 1) effectiveRange = PreyFleeRange;

        if (d < effectiveRange) {
            if (type == 1 && otherType == 2) {
                predForce += normalize(-diff);
                predCount++;
            } else if (type == 2 && otherType == 1) {
                preyForce += normalize(diff);
                preyCount++;
            }

            if (otherType == type || (type == 0 && otherType == 0)) {
                aliForce += otherVel;
                cohForce += otherPos;
                neighborCount++;

                if (d < desiredSep) {
                    sepForce += normalize(diff) / d;
                    sepCount++;
                }
            }
        }
    }

    float3 acc = 0;

    if (neighborCount > 0) {
        float inv = 1.0 / neighborCount;
        aliForce *= inv; cohForce *= inv;
        acc += steer(aliForce, vel, maxSpeed, maxForce) * AlignmentWeight;
        acc += seek(pos, vel, cohForce, maxSpeed, maxForce) * CohesionWeight;
    }
    if (sepCount > 0) {
        sepForce /= sepCount;
        acc += steer(sepForce, vel, maxSpeed, maxForce) * effSepWeight;
    }
    if (predCount > 0 && type == 1) {
        predForce /= predCount;
        acc += steer(predForce, vel, PredatorMaxSpeed, PredatorMaxForce) * PredatorWeight;
    }
    if (preyCount > 0 && type == 2) {
        preyForce /= preyCount;
        acc += steer(preyForce, vel, PreyMaxSpeed, PreyMaxForce) * PreyWeight;
    }

    acc += avoidObstacles(pos, vel, maxSpeed, neighborDist) * ObstacleWeight;

    if (HasTarget > 0) {
        float3 target = float3(TargetX, TargetY, TargetZ);
        acc += seek(pos, vel, target, maxSpeed, maxForce) * TargetWeight;
    }

    acc += avoidBounds(pos, vel, float3(BoundsX, BoundsY, BoundsZ), maxSpeed) * 2.0;

    float effMaxSpeed = maxSpeed;
    float effMaxForce = maxForce;
    if (type == 1) { effMaxSpeed = PredatorMaxSpeed; effMaxForce = PredatorMaxForce; }
    else if (type == 2) { effMaxSpeed = PreyMaxSpeed; effMaxForce = PreyMaxForce; }

    vel += acc;
    vel = limit(vel, effMaxSpeed);
    pos += vel * (DeltaTime * 60.0);

    // Pack back
    boid.posType = float4(pos, asfloat(type));
    boid.velSpeed = float4(vel, effMaxSpeed);
    boid.params = float4(effMaxForce, neighborDist, desiredSep, size);
    boid.color = float4(col, 0);

    g_Boids[idx] = boid;
}
)";

struct BoidsGPUCompute::Impl {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pAgentBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pObstacleBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pConstantBuffer_;
};

BoidsGPUCompute::BoidsGPUCompute(GpuContext& context)
    : context_(context), executor_(context), pImpl_(new Impl()) {}

BoidsGPUCompute::~BoidsGPUCompute() { delete pImpl_; }

void BoidsGPUCompute::initialize(size_t maxAgents) {
    maxAgents_ = maxAgents;
    createBuffers();
    createPSO();
}

void BoidsGPUCompute::createBuffers() {
    auto pDevice = context_.RenderDevice();

    // 1. RWStructuredBuffer for agents (UAV + SRV)
    BufferDesc buffDesc;
    buffDesc.Name = "Boids Agent Buffer (UAV)";
    buffDesc.Usage = USAGE_DEFAULT;
    buffDesc.Size = sizeof(float) * 16 * maxAgents_; // BoidData = ~16 floats
    buffDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    buffDesc.Mode = BUFFER_MODE_STRUCTURED;
    pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->pAgentBuffer_);

    // 2. StructuredBuffer for obstacles (SRV only)
    buffDesc.Name = "Boids Obstacle Buffer (SRV)";
    buffDesc.Usage = USAGE_DEFAULT;
    buffDesc.Size = sizeof(float) * 4 * 64; // max 64 obstacles
    buffDesc.BindFlags = BIND_SHADER_RESOURCE;
    buffDesc.Mode = BUFFER_MODE_STRUCTURED;
    pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->pObstacleBuffer_);

    // 3. Constant buffer
    buffDesc.Name = "Boids Constants CB";
    buffDesc.Usage = USAGE_DYNAMIC;
    buffDesc.Size = sizeof(GpuBoidConstants);
    buffDesc.Mode = BUFFER_MODE_UNDEFINED;
    pDevice->CreateBuffer(buffDesc, nullptr, &pImpl_->pConstantBuffer_);
}

void BoidsGPUCompute::createPSO() {
    static ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_COMPUTE, "g_Boids", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Obstacles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    };

    ComputePipelineDesc desc;
    desc.name = "Boids Compute PSO";
    desc.shaderSource = BoidsUpdateCSSource;
    desc.entryPoint = "CSMain";
    desc.variables = vars;
    desc.variableCount = 2;
    desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (!executor_.build(desc)) {
        qWarning() << "[BoidsGPUCompute] PSO creation FAILED";
        return;
    }

    executor_.setBuffer("Constants", pImpl_->pConstantBuffer_);
    executor_.createShaderResourceBinding(true);
}

void BoidsGPUCompute::uploadAgents(const std::vector<float3>& positions,
                                    const std::vector<float3>& velocities,
                                    const std::vector<GpuAgentType>& types) {
    if (!pImpl_->pAgentBuffer_) return;
    size_t count = std::min(positions.size(), maxAgents_);
    if (count == 0) return;

    // Pack into BoidData (4 x float4 = 16 floats per agent)
    // posType: x,y,z,type(uint→float)
    // velSpeed: x,y,z,maxSpeed
    // params:  maxForce,neighborDistance,desiredSeparation,size
    // color:   r,g,b,unused
    struct PackedBoid { float data[16]; };
    std::vector<PackedBoid> packed(count);
    for (size_t i = 0; i < count; ++i) {
        auto& p = packed[i];
        p.data[0] = positions[i].x;
        p.data[1] = positions[i].y;
        p.data[2] = positions[i].z;
        float typeFloat = 0.0f;
        if (i < types.size()) {
            uint32_t ut = static_cast<uint32_t>(types[i]);
            memcpy(&typeFloat, &ut, sizeof(float));
        }
        p.data[3] = typeFloat;             // posType.w = type as float bits
        p.data[4] = velocities[i].x;
        p.data[5] = velocities[i].y;
        p.data[6] = velocities[i].z;
        p.data[7] = 5.0f;                  // maxSpeed
        p.data[8] = 0.1f;                  // maxForce
        p.data[9] = 50.0f;                 // neighborDistance
        p.data[10] = 25.0f;                // desiredSeparation
        p.data[11] = 4.0f;                 // size
        p.data[12] = 0.3f;                 // color.r
        p.data[13] = 0.6f;                 // color.g
        p.data[14] = 1.0f;                 // color.b
        p.data[15] = 0;
    }

    auto pContext = context_.DeviceContext();
    pContext->UpdateBuffer(pImpl_->pAgentBuffer_, 0, sizeof(PackedBoid) * count,
                           packed.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    constants_.agentCount = static_cast<uint32_t>(count);
}

void BoidsGPUCompute::uploadObstacles(const std::vector<float3>& centers,
                                       const std::vector<float>& radii) {
    if (!pImpl_->pObstacleBuffer_) return;
    size_t count = std::min(centers.size(), static_cast<size_t>(64));
    if (count == 0) { obstacleCount_ = 0; constants_.obstacleCount = 0; return; }

    struct PackedObstacle { float data[4]; };
    std::vector<PackedObstacle> packed(count);
    for (size_t i = 0; i < count; ++i) {
        packed[i].data[0] = centers[i].x;
        packed[i].data[1] = centers[i].y;
        packed[i].data[2] = centers[i].z;
        packed[i].data[3] = radii[i];
    }

    auto pContext = context_.DeviceContext();
    pContext->UpdateBuffer(pImpl_->pObstacleBuffer_, 0, sizeof(PackedObstacle) * count,
                           packed.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    obstacleCount_ = static_cast<uint32_t>(count);
    constants_.obstacleCount = obstacleCount_;
}

void BoidsGPUCompute::setConstants(const GpuBoidConstants& constants) {
    constants_ = constants;
}

void BoidsGPUCompute::dispatch(IDeviceContext* pContext, float dt) {
    constants_.deltaTime = dt;
    constants_.agentCount = static_cast<uint32_t>(maxAgents_);

    void* pData = nullptr;
    pContext->MapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        std::memcpy(pData, &constants_, sizeof(GpuBoidConstants));
        pContext->UnmapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE);
    }

    executor_.setBufferView("g_Boids",
        pImpl_->pAgentBuffer_->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    executor_.setBufferView("g_Obstacles",
        pImpl_->pObstacleBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    DispatchComputeAttribs attribs;
    attribs.ThreadGroupCountX = (static_cast<uint32_t>(maxAgents_) + 63) / 64;
    executor_.dispatch(pContext, attribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

IBuffer* BoidsGPUCompute::getAgentBuffer() {
    return pImpl_->pAgentBuffer_;
}

} // namespace ArtifactCore
