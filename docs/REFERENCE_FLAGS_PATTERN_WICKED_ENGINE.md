# Flags Pattern Reference (from Wicked Engine wiScene_Components.h)

> Source: https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiScene_Components.h
> Target: ParticleSystem state, Layer state, all component state management

## Pattern

```cpp
// CommonInclude.h - helper template
template<typename T>
constexpr void set_flag(T& flags, T flag, bool value) {
    if (value) flags |= flag; else flags &= ~flag;
}

// Component definition
struct TransformComponent {
    enum FLAGS {
        EMPTY = 0,
        DIRTY = 1 << 0,         // ワールド行列の再計算が必要
    };
    uint32_t _flags = DIRTY;

    // Serialized state:
    XMFLOAT3 scale_local     = {1,1,1};
    XMFLOAT4 rotation_local  = {0,0,0,1};  // quaternion
    XMFLOAT3 translation_local = {0,0,0};

    // Non-serialized (runtime only):
    XMFLOAT4X4 world;  // cached world matrix

    constexpr void SetDirty(bool value = true) { set_flag(_flags, DIRTY, value); }
    constexpr bool IsDirty() const { return _flags & DIRTY; }
};

// Bit positions for up to 32 flags
struct ParticleSystem {
    enum FLAGS {
        EMPTY                = 0,
        PAUSED               = 1 << 0,
        SORT_ENABLED         = 1 << 1,
        DEPTH_COLLISION      = 1 << 2,
        SPH_FLUID_ENABLED    = 1 << 3,
        FRAME_BLENDING       = 1 << 4,
        VOLUME_RENDER        = 1 << 5,
        COLLIDERS_DISABLED   = 1 << 6,
        USE_RAIN_BLOCKER     = 1 << 7,
        TAKE_COLOR_FROM_MESH = 1 << 8,
    };
    uint32_t _flags = EMPTY;

    constexpr bool IsPaused() const { return _flags & PAUSED; }
    constexpr void SetPaused(bool v) { set_flag(_flags, PAUSED, v); }
    // ...
};
```

## Why This Pattern?

1. **シリアライズが単一メンバ**: `archive << _flags;` だけで全状態を保存
2. **メモリ効率**: 32 bool → 4 bytes（8倍の圧縮）
3. **コピー効率**: `memcpy` 一発
4. **比較効率**: 単一整数比較 `a._flags == b._flags`
5. **アトミック操作可能**: `std::atomic<uint32_t>` で lock-free 状態変更
6. **constexpr 対応**: コンパイル時にフラグチェック可能

## Wicked Engine Components Using This Pattern

| Component | Flag count | Flags |
|---|---|---|
| TransformComponent | 1 | DIRTY |
| ObjectComponent | 5+ | CAST_SHADOW, LIGHTMAP, IMPOSTOR, etc. |
| RigidBodyPhysics | 3 | DISABLE_DEACTIVATION, KINEMATIC, TRIGGER |
| SoftBodyPhysics | 3 | SAFE_TO_REGISTER, DISABLE_DEACTIVATION, WIND |
| LightComponent | 5 | CAST_SHADOW, VOLUMETRICS, VISUALIZER, etc. |
| CameraComponent | 1 | PLANAR_REFLECTION |
| EmittedParticle | 10 | PAUSED, SORTING, DEPTHCOLLISION, SPH, etc. |
| HairParticle | 4 | REBUILD_BUFFERS, DIRTY, CAMERA_BEND |
| WeatherComponent | 7+ | OCEAN, RAIN, SNOW, LIGHTNING, etc. |

## Alignment Pattern

Wicked Engine は flags + 頻繁アクセスメンバを前方に配置し、
非シリアライズ（= ランタイムキャッシュ）を後方に配置:

```cpp
struct alignas(16) TransformComponent {
    // === Serialized (persistent) ===
    XMFLOAT3 scale_local;        // 12 bytes
    uint32_t _flags;             // 4 bytes  ← ここで16byte境界
    XMFLOAT4 rotation_local;     // 16 bytes
    XMFLOAT3 translation_local;  // 12 bytes
    float _padding;              // 4 bytes  ← パディング補完
    // === Non-serialized (runtime cache) ===
    XMFLOAT4X4 world;            // 64 bytes
};
```

## ArtifactCore Adaptations

- `set_flag()` テンプレートを `Utils/BitFlags.hpp` に配置
- ParticleSystem の個別 bool (`paused_`, `sortEnabled_`, etc.) → `_flags` に統合
- Layer の状態 (visible, locked, solo, adjustment) → `_flags` に統合
- 既存の `LayerState` クラスは flags ラッパーとして存続可能