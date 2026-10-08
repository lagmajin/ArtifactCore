module;
#include <utility>
#include <vector>
#include <array>
#include <cstdint>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Graphics.ParticleData;

import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

// App 層の Config に依存せず Core 内部で完結させるための定数
constexpr auto DefaultParticleRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;

/**
 * @brief GPU レンダリング用の単一パーティクル頂点データ
 *
 * position / velocity / prevPosition は xyz + padding の float4 配置で
 * 96 バイト stride とする。
 * float3 を連続配置すると DXIL ではタイト・Vulkan(std430) では 16 バイト
 * アラインになり、バックエンド毎に color/size の読み位置がずれる（オレンジが
 * シアンに見える・形状が明滅する原因）。HLSL 側も float4 で受ける。
 * prevPosition は trail 線分用でシェーダは読まないが、stride 一致のため置く。
 */
struct ParticleVertex {
    float px, py, pz; // Position
    float pad0 = 0.0f; // Padding (position.w, shader-ignored)
    float vx, vy, vz; // Velocity
    float pad1 = 0.0f; // Padding (velocity.w, shader-ignored)
    float r, g, b, a; // Color (RGBA)
    float size = 1.0f;
    float stretch = 1.0f;
    float rotation = 0.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
    int spriteFrame = 0;
    int spriteRows = 1;
    int spriteCols = 1;
    float ppx = 0.0f, ppy = 0.0f, ppz = 0.0f; // Previous position (trails)
    float pad2 = 0.0f; // Padding (prevPosition.w, shader-ignored)
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
    // Row-major transform consumed by the particle shader.  2D callers leave
    // this as identity because their coordinates are already canvas-space.
    std::array<float, 16> modelMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace ArtifactCore
