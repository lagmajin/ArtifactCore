module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <string>


export module Animation.Dynamics;

// ============================================================
// M-FE-10 Animation Dynamics Core — Phase 1: Solver Primitives
// ============================================================
// Design principles:
//   1. Stateless update functions: f(state, target, dt) -> state
//   2. No global mutable state
//   3. Deterministic: same inputs always yield same outputs
//   4. Numerically stable for typical animation dt ranges (1/24 - 1/240)
//   5. Zero physics-engine dependencies
// ============================================================

export namespace ArtifactCore {

// ────────────────────────────────────────────
// 2D / 3D lightweight vector helpers (no Qt dependency in Core)
// ────────────────────────────────────────────
struct DynVec2 {
    float x = 0.0f, y = 0.0f;
    DynVec2 operator+(const DynVec2& o) const noexcept { return {x + o.x, y + o.y}; }
    DynVec2 operator-(const DynVec2& o) const noexcept { return {x - o.x, y - o.y}; }
    DynVec2 operator*(float s) const noexcept { return {x * s, y * s}; }
};

struct DynVec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    DynVec3 operator+(const DynVec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    DynVec3 operator-(const DynVec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    DynVec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
};

// ────────────────────────────────────────────
// DynamicsPreset — named tuning bundles
// ────────────────────────────────────────────
struct DynamicsPreset {
    float stiffness;  // spring constant k  [N/m]
    float damping;    // damping coefficient c [N·s/m]
    float mass;       // virtual mass m [kg]

    /// Classic smooth follow (critically damped feel)
    static constexpr DynamicsPreset Smooth()  noexcept { return {80.0f,  16.0f, 1.0f}; }
    /// Bouncy / elastic overshoot
    static constexpr DynamicsPreset Bouncy()  noexcept { return {200.0f,  8.0f, 1.0f}; }
    /// Jelly-like soft oscillation
    static constexpr DynamicsPreset Jelly()   noexcept { return {120.0f,  6.0f, 1.2f}; }
    /// Heavy / slow momentum build-up
    static constexpr DynamicsPreset Heavy()   noexcept { return { 40.0f, 20.0f, 3.0f}; }
    /// Floaty / very low gravity feel
    static constexpr DynamicsPreset Floaty()  noexcept { return { 20.0f,  4.0f, 0.5f}; }
    /// Rigid / near-instant snap
    static constexpr DynamicsPreset Rigid()   noexcept { return {500.0f, 50.0f, 1.0f}; }
};

// ============================================================
// 1-D Spring-Damper
// ============================================================
// State for a single scalar channel.
struct SpringDamper1DState {
    float position  = 0.0f;
    float velocity  = 0.0f;
    bool  seeded    = false;  // true once first update has run
};

/// Update one frame.
/// @param state    Mutable solver state (persists across frames).
/// @param target   Desired position this frame (keyframe-evaluated value).
/// @param dt       Time step in seconds.
/// @param preset   Spring parameters.
/// @returns        New position (also written into state.position).
inline float springDamper1D(
    SpringDamper1DState& state,
    float target,
    float dt,
    const DynamicsPreset& preset = DynamicsPreset::Smooth()) noexcept
{
    if (!state.seeded) {
        state.position = target;
        state.velocity = 0.0f;
        state.seeded   = true;
        return target;
    }

    // Semi-implicit Euler (stable for underdamped and overdamped cases):
    //   F = -k*(x - target) - c*v
    //   a = F / m
    //   v += a * dt
    //   x += v * dt
    const float dt_clamped = std::clamp(dt, 0.0001f, 0.1f);
    const float force = -preset.stiffness * (state.position - target)
                      -  preset.damping   *  state.velocity;
    const float acc   = force / std::max(preset.mass, 0.001f);
    state.velocity   += acc * dt_clamped;
    state.position   += state.velocity * dt_clamped;

    return state.position;
}

/// Reset solver state so scrubbing doesn't leave residue.
inline void resetSpringDamper1D(SpringDamper1DState& state) noexcept {
    state = {};
}

// ============================================================
// 2-D Spring-Damper  (per-channel independent)
// ============================================================
struct SpringDamper2DState {
    SpringDamper1DState x, y;
};

inline DynVec2 springDamper2D(
    SpringDamper2DState& state,
    DynVec2 target,
    float dt,
    const DynamicsPreset& preset = DynamicsPreset::Smooth()) noexcept
{
    return {
        springDamper1D(state.x, target.x, dt, preset),
        springDamper1D(state.y, target.y, dt, preset),
    };
}

inline void resetSpringDamper2D(SpringDamper2DState& state) noexcept { state = {}; }

// ============================================================
// 3-D Spring-Damper
// ============================================================
struct SpringDamper3DState {
    SpringDamper1DState x, y, z;
};

inline DynVec3 springDamper3D(
    SpringDamper3DState& state,
    DynVec3 target,
    float dt,
    const DynamicsPreset& preset = DynamicsPreset::Smooth()) noexcept
{
    return {
        springDamper1D(state.x, target.x, dt, preset),
        springDamper1D(state.y, target.y, dt, preset),
        springDamper1D(state.z, target.z, dt, preset),
    };
}

inline void resetSpringDamper3D(SpringDamper3DState& state) noexcept { state = {}; }

// ============================================================
// Lag Follower (first-order low-pass / exponential smooth)
// ============================================================
// Simpler than spring-damper: no overshoot possible.
// Good for camera follow, secondary motion, subtle inertia.
// tau = time constant in seconds (larger = slower response).
struct LagFollower1DState {
    float value  = 0.0f;
    bool  seeded = false;
};

inline float lagFollower1D(
    LagFollower1DState& state,
    float target,
    float dt,
    float tau = 0.1f) noexcept
{
    if (!state.seeded) {
        state.value  = target;
        state.seeded = true;
        return target;
    }
    const float dt_clamped = std::clamp(dt, 0.0001f, 0.5f);
    const float alpha      = dt_clamped / (std::max(tau, 0.001f) + dt_clamped);
    state.value           += alpha * (target - state.value);
    return state.value;
}

inline void resetLagFollower1D(LagFollower1DState& state) noexcept { state = {}; }

struct LagFollower2DState { LagFollower1DState x, y; };
inline DynVec2 lagFollower2D(LagFollower2DState& s, DynVec2 t, float dt, float tau = 0.1f) noexcept {
    return { lagFollower1D(s.x, t.x, dt, tau), lagFollower1D(s.y, t.y, dt, tau) };
}
inline void resetLagFollower2D(LagFollower2DState& s) noexcept { s = {}; }

struct LagFollower3DState { LagFollower1DState x, y, z; };
inline DynVec3 lagFollower3D(LagFollower3DState& s, DynVec3 t, float dt, float tau = 0.1f) noexcept {
    return { lagFollower1D(s.x, t.x, dt, tau), lagFollower1D(s.y, t.y, dt, tau), lagFollower1D(s.z, t.z, dt, tau) };
}
inline void resetLagFollower3D(LagFollower3DState& s) noexcept { s = {}; }

// ============================================================
// Overshoot Limiter
// ============================================================
// Clamps how far past the target a spring is allowed to travel.
// Applied post-update to prevent visually jarring overshoots.
// overshootFactor:  0.0 = no overshoot (like clamped lag),
//                   1.0 = 100% of distance from rest, etc.
inline float clampOvershoot(
    float currentPos,
    float targetPos,
    float restPos,
    float overshootFactor = 0.25f) noexcept
{
    const float allowedRange = std::abs(targetPos - restPos) * overshootFactor;
    const float lo = std::min(targetPos, restPos) - allowedRange;
    const float hi = std::max(targetPos, restPos) + allowedRange;
    return std::clamp(currentPos, lo, hi);
}

// ============================================================
// Velocity Accumulator
// ============================================================
// Estimates instantaneous velocity given a sequence of position samples.
// Useful for follow-through / drag-on-release effects.
struct VelocityAccumulator1D {
    float prevPos = 0.0f;
    float velocity = 0.0f;

    float update(float newPos, float dt) noexcept {
        const float dt_clamped = std::max(dt, 0.0001f);
        velocity = (newPos - prevPos) / dt_clamped;
        prevPos  = newPos;
        return velocity;
    }

    void reset(float pos = 0.0f) noexcept {
        prevPos  = pos;
        velocity = 0.0f;
    }
};

struct VelocityAccumulator2D {
    VelocityAccumulator1D x, y;

    DynVec2 update(DynVec2 pos, float dt) noexcept {
        return { x.update(pos.x, dt), y.update(pos.y, dt) };
    }
    void reset(DynVec2 pos = {}) noexcept { x.reset(pos.x); y.reset(pos.y); }
};

// ============================================================
// Convenience: DynamicsChannel
// Wraps spring + lag choice + overshoot in one package.
// ============================================================
enum class DynamicsMode {
    Off,
    Spring,    // SpringDamper – supports overshoot
    LagFollow, // Exponential smooth – no overshoot
};

struct DynamicsChannel1D {
    DynamicsMode      mode           = DynamicsMode::Off;
    DynamicsPreset    preset         = DynamicsPreset::Smooth();
    float             lagTau         = 0.1f;
    float             overshootLimit = 0.3f; // fraction of total travel
    bool              clampOvershootEnabled = false;

    SpringDamper1DState  springState;
    LagFollower1DState   lagState;

    float restPos = 0.0f; // starting position (set when seeded)

    /// Evaluate one frame.  target = keyframe-evaluated canonical value.
    float update(float target, float dt) noexcept {
        switch (mode) {
            case DynamicsMode::Off:
                return target;
            case DynamicsMode::Spring: {
                float out = springDamper1D(springState, target, dt, preset);
                if (clampOvershootEnabled)
                    out = clampOvershoot(out, target, restPos, overshootLimit);
                return out;
            }
            case DynamicsMode::LagFollow:
                return lagFollower1D(lagState, target, dt, lagTau);
        }
        return target;
    }

    /// Call this when scrubbing / seeking to avoid state bleed.
    void reset(float snappedPosition) noexcept {
        restPos          = snappedPosition;
        springState      = {};
        lagState         = {};
    }
};

/// Returns a built-in preset by name ("Smooth", "Bouncy", "Jelly", "Heavy", "Floaty", "Rigid").
/// Falls back to Smooth for unknown names.
export DynamicsPreset presetByName(const std::string& name);

/// Returns a human-readable name for a preset.
/// Returns "Custom" if the parameters don't match any built-in preset.
export const char* presetName(const DynamicsPreset& p) noexcept;

} // namespace ArtifactCore
