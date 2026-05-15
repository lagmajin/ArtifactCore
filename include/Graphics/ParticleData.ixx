module;
#include <utility>
#include <vector>
#include <cstdint>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Graphics.ParticleData;

import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

// App 層の Config に依存せず Core 内部で完結させるための定数
constexpr auto DefaultParticleRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;

/**
 * @brief GPU レンダリング用の単一パーティクル頂点データ
 */
struct ParticleVertex {
    float px, py, pz; // Position
    float vx, vy, vz; // Velocity
    float r, g, b, a; // Color (RGBA)
    float size = 1.0f;
    float rotation = 0.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
};

/**
 * @brief レンダラーへ渡すスナップショットデータ
 */
struct ParticleRenderData {
    std::vector<ParticleVertex> particles;
    int64_t frameNumber = 0;
};

} // namespace ArtifactCore
