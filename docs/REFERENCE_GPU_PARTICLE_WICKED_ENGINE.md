# ShaderInterop / GPU Particle Reference (from Wicked Engine)

> Source: https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ShaderInterop_EmittedParticle.h
>        https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiEmittedParticle.h
> Target: C-SIM-1 Pyro Simulation, C-GFX-3 Shader Utility Cleanup, ParticleSystem GPU backend

## CPU/GPU Shared Structure Pattern

Wicked Engine uses a `ShaderInterop_*.h` header pattern where CPU and GPU
share the same struct definitions. This avoids duplication and guarantees
layout compatibility.

### ShaderInterop_EmittedParticle.h (CPU + HLSL)

```cpp
// This header is #included from both C++ and HLSL via #ifdef __cplusplus guards

struct Particle {
    float3 position;     // 12 bytes
    float mass;          // 4 bytes  → 16 byte aligned
    float3 force;        // 12 bytes
    uint rotation_rotationVelocity; // 4 bytes packed (rotation:16bit | vel:16bit)
    float3 velocity;     // 12 bytes
    float maxLife;       // 4 bytes
    float2 sizeBeginEnd; // 8 bytes
    float life;          // 4 bytes
    uint color;          // 4 bytes → 32 byte aligned
}; // Total: 64 bytes per particle (cache-line friendly)

struct ParticleCounters {
    uint aliveCount;
    uint deadCount;
    uint realEmitCount;
    uint aliveCount_afterSimulation;
    uint culledCount;
    uint cellAllocator;
};

// Constant buffer shared between CPU upload and GPU read
CBUFFER(EmittedParticleCB, CBSLOT_OTHER_EMITTEDPARTICLE) {
    uint  xEmitBufferOffset;
    float xEmitterRandomness;
    float xParticleSize;
    // ... all emitter parameters ...
    float3 xParticleGravity;
    float  xEmitterRestitution;
    float3 xParticleVelocity;
    float  xParticleDrag;
    // SPH parameters
    float xSPH_h, xSPH_h_rcp, xSPH_h2, xSPH_h3;
    float xSPH_poly6_constant, xSPH_spiky_constant, xSPH_visc_constant;
    float xSPH_K, xSPH_e, xSPH_p0;
};
```

### CPU-side Class (wiEmittedParticle.h)

```cpp
class EmittedParticleSystem {
    // GPU Buffers
    GPUBuffer particleBuffer;         // RWStructuredBuffer<Particle>
    GPUBuffer aliveList[2];           // double-buffered alive indices
    GPUBuffer deadList;               // freelist
    GPUBuffer counterBuffer;          // indirect dispatch counters
    GPUBuffer indirectBuffers;        // Dispatch + Draw indirect args
    GPUBuffer constantBuffer;         // EmittedParticleCB
    GPUBuffer distanceBuffer;         // for sorting
    GPUBuffer sphGridCells;           // SPH acceleration grid
    GPUBuffer sphParticleCells;       // SPH particle→cell mapping

    // CPU state (emitter parameters)
    float emit = 0.0f;      // emission rate
    int burst = 0;
    float dt = 0;
    uint32_t MAX_PARTICLES = 1000;
    uint32_t _flags;

    float size, random_factor, normal_factor, count, life;
    float scaleX, scaleY, rotation, motionBlurAmount, mass;
    float random_color, drag, restitution;
    XMFLOAT3 velocity, gravity;

public:
    void UpdateCPU(const TransformComponent& transform, float dt);
    void Burst(int num, const XMFLOAT3& position, const Color& color);
    void UpdateGPU(uint32_t instanceIndex, const MeshComponent* mesh, CommandList cmd) const;
    void Draw(const MaterialComponent& material, CommandList cmd) const;
    static void Initialize(); // One-time GPU PSO creation
};
```

## GPU Pipeline Stages

```
1. UpdateCPU(transform, dt)
   → Compute emit count, fill EmittedParticleCB constant buffer
   → Upload CB to GPU

2. UpdateGPU(instanceIndex, mesh, cmd)
   ┌─ Emit CS: deadList → aliveList (spawn new particles)
   ├─ Simulation CS: aliveList[0] → aliveList[1]
   │   - Apply gravity, wind, drag
   │   - Apply force fields
   │   - SPH fluid (density → pressure → velocity)
   │   - Depth collision (sample depth buffer)
   ├─ Sort CS: Bitonic sort by distance to camera
   └─ Culling CS: Frustum culling → compact alive list

3. Draw(material, cmd)
   → IndirectDrawInstanced from culled list
   → Billboard expansion in vertex shader
   → Soft particle blending in pixel shader
```

## SPH Fluid on GPU

```cpp
// Grid-based neighbor search (27 neighbors)
static const int3 sph_neighbor_offsets[27] = {
    int3(-1,-1,-1), int3(-1,-1,0), int3(-1,-1,1),
    int3(-1, 0,-1), int3(-1, 0,0), int3(-1, 0,1),
    // ... 27 total
};

inline uint SPH_GridHash(int3 cellIndex) {
    const uint p1 = 73856093, p2 = 19349663, p3 = 83492791;
    int n = p1*cellIndex.x ^ p2*cellIndex.y ^ p3*cellIndex.z;
    n %= SPH_PARTITION_BUCKET_COUNT;
    return n;
}

struct SPHGridCell { uint count; uint offset; };
```

## ArtifactCore Adaptations

1. **ShaderInterop ヘッダ**: `ArtifactCore/shaders/` に CPU/GPU 共有ヘッダを配置
2. **Particle struct**: 既存 `Particle` 型（128 bytes）を 64 bytes に縮小検討
   - `float4 custom0/custom1` → 不要なら削除
   - `rotation` + `angularVelocity` → `rotation_rotationVelocity` にパック
3. **定数バッファ**: バラバラのパラメータ → `ParticleSystemCB` に集約
4. **SPH グリッド**: `C-SIM-1 Pyro Simulation` の GPU 実装時に直接再利用可能
5. **間接描画**: `indirectBuffers` パターンで CPU readback 不要の GPU-driven パイプライン