module;
#include <cstdint>
#include <vector>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include "../Define/DllExportMacro.hpp"

export module Graphics.MpmCompute;

import Graphics.GPUcomputeContext;
import Graphics.Compute;

export namespace ArtifactCore {

using namespace Diligent;

// Packed particle layout mirrored by the HLSL kernels (22 floats, 88 bytes).
// Keep field order in sync with MpmParticleHLSL inside MpmCompute.cppm.
struct MpmGpuParticle {
    float posX = 0.0f, posY = 0.0f;
    float velX = 0.0f, velY = 0.0f;
    float mass = 0.0f, volume0 = 0.0f;
    float F00 = 1.0f, F01 = 0.0f, F10 = 0.0f, F11 = 1.0f;
    float Fp00 = 1.0f, Fp01 = 0.0f, Fp10 = 0.0f, Fp11 = 1.0f;
    float C00 = 0.0f, C01 = 0.0f, C10 = 0.0f, C11 = 0.0f;
    float plasticStrain = 0.0f;
    float active = 1.0f;   // 1.0 = active, 0.0 = fractured
    float pad0 = 0.0f, pad1 = 0.0f;
};

// Constant buffer layout shared with HLSL (7 x 16-byte registers).
struct MpmGpuParams {
    std::uint32_t particleCount = 0;
    std::uint32_t nodeCount = 0;
    std::uint32_t gridNx = 0;
    std::uint32_t gridNy = 0;
    float cellSize = 1.0f;
    float originX = 0.0f;
    float originY = 0.0f;
    float invCellSize = 1.0f;
    float dt = 1.0e-4f;
    float gravityX = 0.0f;
    float gravityY = 980.0f;
    float mu = 0.0f;
    float lambda = 0.0f;
    float damping = 0.0f;
    float boundaryFriction = 0.0f;
    float bxmin = 0.0f;
    float bxmax = 100.0f;
    float bymin = 0.0f;
    float bymax = 100.0f;
    float hasBoundary = 0.0f;
};

// GPU compute lane for MpmSolver2D following the SandGPUCompute pattern:
// upload particles -> per-substep dispatch chain -> readback particles.
// Grid state is scratch per substep (mass/momentum/force accumulate via
// ordered-int atomic adds). Plasticity/fracture/colliders stay on the CPU
// and run once after readback, so results are not bit-identical to the CPU
// lane; treat both as separate lanes until parity is demonstrated.
class LIBRARY_DLL_API MpmGPUCompute {
public:
    static constexpr int kParticleFloats = 22;

    explicit MpmGPUCompute(GpuContext& context);
    ~MpmGPUCompute();

    bool initialize(std::uint32_t gridNx, std::uint32_t gridNy,
                    std::uint32_t maxParticles);

    void uploadParticles(IDeviceContext* pContext,
                         const MpmGpuParticle* particles,
                         std::uint32_t count);
    void simulateSubsteps(IDeviceContext* pContext, int substeps,
                          const MpmGpuParams& params);
    void readbackParticles(IDeviceContext* pContext,
                           MpmGpuParticle* outParticles,
                           std::uint32_t count);

    std::uint32_t gridNx() const { return nx_; }
    std::uint32_t gridNy() const { return ny_; }
    std::uint32_t capacity() const { return capacity_; }
    bool ready() const { return ready_; }

private:
    bool createBuffers();
    bool buildPipelines();

    GpuContext& context_;
    ComputeExecutor clearExec_;
    ComputeExecutor p2gExec_;
    ComputeExecutor forceExec_;
    ComputeExecutor gridUpdateExec_;
    ComputeExecutor g2pExec_;

    RefCntAutoPtr<IBuffer> pConstantBuffer_;
    RefCntAutoPtr<IBuffer> pParticleBuffer_;
    RefCntAutoPtr<IBuffer> pGridMass_;       // ordered-int encoded floats
    RefCntAutoPtr<IBuffer> pGridMomentum_;   // uint2 per node
    RefCntAutoPtr<IBuffer> pGridForce_;      // uint2 per node

    std::uint32_t nx_ = 0;
    std::uint32_t ny_ = 0;
    std::uint32_t capacity_ = 0;
    std::uint32_t particleCount_ = 0;
    bool ready_ = false;
};

} // namespace ArtifactCore
