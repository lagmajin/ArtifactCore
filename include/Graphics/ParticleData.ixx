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
    float stretch = 1.0f;
    float rotation = 0.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
    int spriteFrame = 0;
    int spriteRows = 1;
    int spriteCols = 1;
};

enum class ParticleBlendPolicy : std::uint32_t {
    Additive = 0,
    Subtractive = 1,
    Alpha = 2,
    Screen = 3,
    Multiply = 4
};

enum class ParticleBillboardPolicy : std::uint32_t {
    None = 0,
    ScreenAligned = 1,
    ViewPlane = 2,
    VelocityAligned = 3
};

struct ParticleRenderOptions {
    ParticleBlendPolicy blend = ParticleBlendPolicy::Additive;
    ParticleBillboardPolicy billboard = ParticleBillboardPolicy::ScreenAligned;
    bool depthTest = false;
    bool depthWrite = false;

    bool operator==(const ParticleRenderOptions&) const = default;
};

/**
 * @brief レンダラーへ渡すスナップショットデータ
 */
struct ParticleRenderData {
    std::vector<ParticleVertex> particles;
    int64_t frameNumber = 0;
    ParticleRenderOptions options;
};

} // namespace ArtifactCore
