module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include "../Define/DllExportMacro.hpp"
#include <vector>

export module Graphics.BoidsCompute;

import Graphics.GPUcomputeContext;
import Graphics.Compute;
import Graphics.ParticleData;

export namespace ArtifactCore {

using namespace Diligent;

enum class GpuAgentType : uint32_t {
    Normal = 0,
    Predator,
    Prey
};

struct GpuBoidConstants {
    float separationWeight;
    float alignmentWeight;
    float cohesionWeight;
    float targetWeight;
    float wanderWeight;
    float obstacleWeight;
    float predatorWeight;
    float preyWeight;
    float deltaTime;
    float wanderJitter;
    float wanderRadius;
    float wanderDistance;
    float predatorMaxSpeed;
    float predatorMaxForce;
    float predatorChaseRange;
    float preyMaxSpeed;
    float preyMaxForce;
    float preyFleeRange;
    float targetX, targetY, targetZ;
    uint32_t hasTarget;
    float boundsX, boundsY, boundsZ;
    uint32_t agentCount;
    uint32_t obstacleCount;
    float _padding[2];
};

/**
 * @brief GPU compute-based boids swarm system.
 * Uses a compute shader with O(N^2) brute-force neighbor search.
 * Suitable for up to ~5000 agents.
 */
class LIBRARY_DLL_API BoidsGPUCompute {
public:
    BoidsGPUCompute(GpuContext& context);
    ~BoidsGPUCompute();

    void initialize(size_t maxAgents);

    void uploadAgents(const std::vector<float3>& positions,
                      const std::vector<float3>& velocities,
                      const std::vector<GpuAgentType>& types);

    void uploadObstacles(const std::vector<float3>& centers,
                         const std::vector<float>& radii);

    void setConstants(const GpuBoidConstants& constants);
    void dispatch(IDeviceContext* pContext, float dt);

    IBuffer* getAgentBuffer();

    size_t maxAgents() const { return maxAgents_; }

private:
    GpuContext& context_;
    ComputeExecutor executor_;
    class Impl;
    Impl* pImpl_ = nullptr;
    size_t maxAgents_ = 0;
    GpuBoidConstants constants_{};
    uint32_t obstacleCount_ = 0;

    void createBuffers();
    void createPSO();
};

} // namespace ArtifactCore
