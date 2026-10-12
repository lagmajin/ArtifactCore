module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <cstddef>
#include <utility>

export module Physics.Mpm2D;

namespace ArtifactCore {

export struct MpmVec2 {
    float x = 0.0f, y = 0.0f;

    MpmVec2() = default;
    constexpr MpmVec2(float x_, float y_) noexcept : x(x_), y(y_) {}

    MpmVec2 operator+(const MpmVec2& o) const noexcept { return {x + o.x, y + o.y}; }
    MpmVec2 operator-(const MpmVec2& o) const noexcept { return {x - o.x, y - o.y}; }
    MpmVec2 operator*(float s) const noexcept { return {x * s, y * s}; }
    MpmVec2 operator/(float s) const noexcept { return {x / s, y / s}; }
    MpmVec2& operator+=(const MpmVec2& o) noexcept { x += o.x; y += o.y; return *this; }
    MpmVec2& operator-=(const MpmVec2& o) noexcept { x -= o.x; y -= o.y; return *this; }
    MpmVec2& operator*=(float s) noexcept { x *= s; y *= s; return *this; }
    MpmVec2& operator/=(float s) noexcept { x /= s; y /= s; return *this; }
    float dot(const MpmVec2& o) const noexcept { return x * o.x + y * o.y; }
    float lengthSq() const noexcept { return x * x + y * y; }
    float length() const noexcept { return std::sqrt(lengthSq()); }
    MpmVec2 normalized() const noexcept {
        float len = length();
        return (len > 1e-10f) ? (*this) / len : MpmVec2{};
    }
};

inline MpmVec2 operator*(float s, const MpmVec2& v) noexcept {
    return v * s;
}

export struct MpmMat2 {
    float m00 = 1.0f, m01 = 0.0f;
    float m10 = 0.0f, m11 = 1.0f;

    MpmMat2() = default;
    constexpr MpmMat2(float a, float b, float c, float d) noexcept
        : m00(a), m01(b), m10(c), m11(d) {}

    static MpmMat2 identity() noexcept;
    static MpmMat2 zero() noexcept;

    float det() const noexcept { return m00 * m11 - m01 * m10; }
    MpmMat2 transpose() const noexcept { return {m00, m10, m01, m11}; }
    MpmMat2 inverse() const noexcept {
        float d = det();
        if (std::abs(d) < 1e-20f) return identity();
        float inv = 1.0f / d;
        return { m11 * inv, -m01 * inv, -m10 * inv, m00 * inv };
    }

    MpmMat2 operator+(const MpmMat2& o) const noexcept { return {m00+o.m00, m01+o.m01, m10+o.m10, m11+o.m11}; }
    MpmMat2 operator-(const MpmMat2& o) const noexcept { return {m00-o.m00, m01-o.m01, m10-o.m10, m11-o.m11}; }
    MpmMat2 operator*(float s) const noexcept { return {m00*s, m01*s, m10*s, m11*s}; }
    MpmMat2 operator*(const MpmMat2& o) const noexcept {
        return {
            m00*o.m00 + m01*o.m10, m00*o.m01 + m01*o.m11,
            m10*o.m00 + m11*o.m10, m10*o.m01 + m11*o.m11
        };
    }
    MpmVec2 operator*(const MpmVec2& v) const noexcept {
        return { m00*v.x + m01*v.y, m10*v.x + m11*v.y };
    }
    MpmMat2& operator+=(const MpmMat2& o) noexcept { m00+=o.m00; m01+=o.m01; m10+=o.m10; m11+=o.m11; return *this; }
    MpmMat2& operator-=(const MpmMat2& o) noexcept { m00-=o.m00; m01-=o.m01; m10-=o.m10; m11-=o.m11; return *this; }
    float frobeniusSq() const noexcept { return m00*m00 + m01*m01 + m10*m10 + m11*m11; }

    // F: m00,m01 = row0 = Fx col; m10,m11 = row1 = Fy col
    //             = [Fx, Fy]  (F * vec = Fx * v.x + Fy * v.y)
    //              -> contiguous layout
};

// ---------- particle ----------

export struct MpmParticle2D {
    MpmVec2  pos;
    MpmVec2  vel;
    float    mass      = 0.0f;
    float    volume0   = 0.0f;  // initial volume
    MpmMat2  F;                  // total deformation gradient
    MpmMat2  Fp;                 // plastic deformation gradient
    MpmMat2  C;                  // APIC affine matrix
    float    plasticStrain = 0.0f;
    bool     active       = true;
    float    r = 0.8f, g = 0.3f, b = 0.2f; // base colour
};

// Axis-aligned bounds of active material points in solver coordinates.
// A caller may provide non-negative padding to include reconstructed surface
// support without changing particle or solver state.
export struct MpmBounds2D {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    bool valid = false;
};

export struct MpmSnapshot2D {
    std::vector<MpmParticle2D> particles;
    std::vector<int> fracturedIndices;
    MpmVec2 gridOrigin;
    float accumulatedTime = 0.0f;
    int particleGridColumns = 0;
    int particleGridRows = 0;
    // Composition-scoped PhysicsSystem remainder; standalone solid snapshots
    // leave this at zero, while granular world snapshots populate it.
    float physicsWorldAccumulator = 0.0f;
    // Granular snapshots retain active grid geometry; restore may change it
    // when the receiver has enough pre-reserved node storage.
    float solverGridCellSize = 1.0f;
    int solverGridColumns = 32;
    int solverGridRows = 32;
    bool gridExpansionLimitReached = false;
};

// Principal-log-strain projection used by the Hencky/Drucker-Prager sand
// constitutive path. This scalar helper is independent of the legacy solid
// material presets and does not mutate solver or particle state.
export struct MpmHenckyProjection2D {
    float logStretch0 = 0.0f;
    float logStretch1 = 0.0f;
    float plasticMultiplier = 0.0f;
    bool projected = false;
    bool extensionTip = false;
    bool valid = false;
};

export MpmHenckyProjection2D projectMpmGranularHencky2D(
    float logStretch0, float logStretch1, float lameLambda,
    float shearModulus, float frictionAlpha) noexcept;

export enum class MpmConstitutiveModel : std::uint8_t {
    CorotatedVonMises,
    GranularDruckerPrager,
};

export struct MpmCollider2D {
    enum class Type : std::uint8_t { Plane, Box, Circle, Polygon };
    Type type = Type::Plane;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    float restitution = 0.1f;
    float friction = 0.2f;
    bool enabled = true;
    // Interleaved outline vertices (x0, y0, x1, y1, ...) used by Type::Polygon.
    std::vector<float> polygonPoints;
    // Solver-derived coefficients and polygon bounds, populated on registration.
    float clampedRestitution = 0.1f;
    float clampedFriction = 0.2f;
    float granularRestitution = 0.1f;
    float granularFriction = 0.2f;
    // Cached exact vertex bounds, populated by MpmSolver2D::addCollider().
    float polygonMinX = 0.0f;
    float polygonMinY = 0.0f;
    float polygonMaxX = 0.0f;
    float polygonMaxY = 0.0f;
    float polygonTwiceArea = 0.0f;
};

// ---------- grid node ----------

struct MpmGridNode {
    float   mass = 0.0f;
    MpmVec2 vel;
    MpmVec2 force;
};

export enum class MpmMaterialPreset : std::uint8_t {
    Flesh,
    Foam,
    HardRubber,
    Wood,
};

export struct MpmParticleGridSeedGeometry2D {
    std::size_t particleCount = 0;
    float stepX = 0.0f;
    float stepY = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    float lastX = 0.0f;
    float lastY = 0.0f;
    float totalVolume = 0.0f;
    float particleVolume = 0.0f;
    float particleMass = 0.0f;
    bool valid = false;
};

// Shared preflight geometry for the public seed-grid API and the granular
// PhysicsSystem creation path, preserving identical float operation order.
export MpmParticleGridSeedGeometry2D calculateMpmParticleGridSeedGeometry2D(
    float centerX, float centerY, float width, float height,
    int columns, int rows, float density, bool granularSand) noexcept;

// ---------- solver ----------

export enum class MpmBackend : std::uint8_t { CPU, GPU };

export class MpmSolver2D {
public:
    MpmSolver2D() = default;
    ~MpmSolver2D();

    // ---- backend ----
    // GPU lane is opt-in: attach shared Diligent device/context pointers
    // first, then switch. Raw void* keeps Diligent types out of this
    // interface module; the implementation wraps them into GpuContext.
    // Any GPU failure falls back to the CPU lane transparently.
    MpmBackend backend() const noexcept { return backend_; }
    bool setBackend(MpmBackend backend) noexcept;
    bool attachGPUSimulation(void* sharedRenderDevice, void* immediateContext);
    void detachGPUSimulation();

    // ---- configuration ----
    // General MPM grid geometry is fixed once particles exist. Granular
    // solvers may expand within node storage reserved before population.
    bool setGrid(float cellSize, int nx, int ny);
    bool reserveGridNodeCapacity(std::size_t maximumNodes);
    std::size_t gridNodeCapacity() const noexcept { return grid_.size(); }
    bool gridExpansionLimitReached() const noexcept {
        return gridExpansionLimitReached_;
    }
    float accumulatedTime() const noexcept { return accumulatedTime_; }
    bool setGridOrigin(float x, float y) noexcept {
        if (!particles_.empty()) return false;
        const double extentX = static_cast<double>(nx_) * cellSize_;
        const double extentY = static_cast<double>(ny_) * cellSize_;
        const double right = static_cast<double>(x) + extentX;
        const double bottom = static_cast<double>(y) + extentY;
        const double maxFloat = std::numeric_limits<float>::max();
        if (std::isfinite(x) && std::isfinite(y) &&
            std::isfinite(right) && std::isfinite(bottom) &&
            std::abs(right) <= maxFloat && std::abs(bottom) <= maxFloat) {
            gridOrigin_ = {x, y};
            return true;
        }
        return false;
    }
    void setMaterial(float youngModulus, float poissonRatio);
    // Select constitutive behavior before populating the solver. Granular
    // Drucker-Prager is CPU-only until its GPU kernels have matching parity.
    bool setConstitutiveModel(MpmConstitutiveModel model) noexcept;
    MpmConstitutiveModel constitutiveModel() const noexcept {
        return constitutiveModel_;
    }
    // Friction angle is an authored material parameter and is immutable while
    // particle state exists; rebuild the solver to change it.
    bool setGranularFrictionAngle(float radians) noexcept;
    float granularFrictionAngle() const noexcept {
        return granularFrictionAngle_;
    }
    void setPlasticity(float yieldStress, float hardening);
    void setFracture(float maxPlasticStrain, float fractureEnergy = 0.0f);
    void setFractureEnabled(bool enabled) noexcept { fractureEnabled_ = enabled; }
    bool fractureEnabled() const noexcept { return fractureEnabled_; }
    void applyMaterialPreset(MpmMaterialPreset preset);
    void setDamping(float damping) { damping_ = std::clamp(damping, 0.0f, 1.0f); }
    float damping() const noexcept { return damping_; }
    void setGravity(float gx, float gy) {
        gravityX_ = std::isfinite(gx) ? gx : 0.0f;
        gravityY_ = std::isfinite(gy) ? gy : 980.0f;
    }
    float gravityX() const noexcept { return gravityX_; }
    float gravityY() const noexcept { return gravityY_; }
    void setTimeStep(float dt) {
        if (std::isfinite(dt) && dt > 0.0f)
            fixedDt_ = std::clamp(dt, 1.0e-6f, 0.1f);
    }
    float timeStep() const noexcept { return fixedDt_; }
    void setMaxSubsteps(int count) { maxSubsteps_ = std::clamp(count, 1, 1024); }
    int maxSubsteps() const noexcept { return maxSubsteps_; }

    // ---- particle population ----
    void addParticlesGrid(float cx, float cy, float w, float h, int cols, int rows, float density = 1000.0f);
    void addParticlesRandom(float cx, float cy, float radius, int count, float density = 1000.0f);
    void clear();

    // ---- stepping ----
    void update(float dt);

    // ---- external forces ----
    void addImpulse(float x, float y, float force, float radius = 5.0f);
    void addBodyForce(float fx, float fy);

    // ---- collision ----
    void setBoundary(float xmin, float ymin, float xmax, float ymax);
    bool hasBoundary() const noexcept { return hasBoundary_; }
    bool setGranularFloor(float y) noexcept;
    bool hasGranularFloor() const noexcept { return hasGranularFloor_; }
    bool setGranularSupportRadiusLimit(float radius) noexcept;
    void setBoundaryFriction(float friction) { boundaryFriction_ = std::clamp(friction, 0.0f, 1.0f); }
    float boundaryFriction() const noexcept { return boundaryFriction_; }
    void addCollider(const MpmCollider2D& collider) {
        const auto isSafeColliderScalar = [](float value) noexcept {
            constexpr float kMaximumColliderCoordinate = 1.0e18f;
            return std::isfinite(value) &&
                   std::abs(value) <= kMaximumColliderCoordinate;
        };
        float polygonMinX = std::numeric_limits<float>::max();
        float polygonMinY = std::numeric_limits<float>::max();
        float polygonMaxX = std::numeric_limits<float>::lowest();
        float polygonMaxY = std::numeric_limits<float>::lowest();
        double polygonTwiceArea = 0.0;
        switch (collider.type) {
        case MpmCollider2D::Type::Plane:
            if (!isSafeColliderScalar(collider.y)) return;
            break;
        case MpmCollider2D::Type::Box:
            if (!isSafeColliderScalar(collider.x) ||
                !isSafeColliderScalar(collider.y) ||
                !isSafeColliderScalar(collider.width) ||
                !isSafeColliderScalar(collider.height)) return;
            break;
        case MpmCollider2D::Type::Circle:
            if (!isSafeColliderScalar(collider.x) ||
                !isSafeColliderScalar(collider.y) ||
                !isSafeColliderScalar(collider.radius)) return;
            break;
        case MpmCollider2D::Type::Polygon:
            if (collider.polygonPoints.size() < 6 ||
                collider.polygonPoints.size() % 2 != 0) return;
            {
                const double areaOriginX = collider.polygonPoints[0];
                const double areaOriginY = collider.polygonPoints[1];
                for (std::size_t i = 0;
                     i < collider.polygonPoints.size(); i += 2) {
                    const float x = collider.polygonPoints[i];
                    const float y = collider.polygonPoints[i + 1];
                    // Contact math squares edge deltas in float. Keep polygon
                    // vertices within the same safe range as other collider
                    // coordinates.
                    if (!isSafeColliderScalar(x) ||
                        !isSafeColliderScalar(y)) return;
                    polygonMinX = std::min(polygonMinX, x);
                    polygonMinY = std::min(polygonMinY, y);
                    polygonMaxX = std::max(polygonMaxX, x);
                    polygonMaxY = std::max(polygonMaxY, y);
                    const std::size_t previous = i == 0
                        ? collider.polygonPoints.size() - 2
                        : i - 2;
                    const float previousX = collider.polygonPoints[previous];
                    const float previousY =
                        collider.polygonPoints[previous + 1];
                    const double currentRelativeX =
                        static_cast<double>(x) - areaOriginX;
                    const double currentRelativeY =
                        static_cast<double>(y) - areaOriginY;
                    const double previousRelativeX =
                        static_cast<double>(previousX) - areaOriginX;
                    const double previousRelativeY =
                        static_cast<double>(previousY) - areaOriginY;
                    polygonTwiceArea +=
                        previousRelativeX * currentRelativeY -
                        currentRelativeX * previousRelativeY;
                }
            }
            break;
        default:
            return;
        }
        colliders_.emplace_back();
        MpmCollider2D& stored = colliders_.back();
        stored.type = collider.type;
        stored.x = collider.x;
        stored.y = collider.y;
        stored.width = collider.width;
        stored.height = collider.height;
        stored.radius = collider.radius;
        stored.restitution = collider.restitution;
        stored.friction = collider.friction;
        stored.enabled = collider.enabled;
        stored.clampedFriction = std::clamp(collider.friction, 0.0f, 1.0f);
        stored.clampedRestitution = std::clamp(
            collider.restitution, 0.0f, 1.0f);
        stored.granularFriction = std::isfinite(collider.friction)
            ? stored.clampedFriction : 0.0f;
        stored.granularRestitution = std::isfinite(collider.restitution)
            ? stored.clampedRestitution : 0.0f;
        if (collider.type == MpmCollider2D::Type::Polygon) {
            stored.polygonMinX = polygonMinX;
            stored.polygonMinY = polygonMinY;
            stored.polygonMaxX = polygonMaxX;
            stored.polygonMaxY = polygonMaxY;
            const double maximumArea =
                static_cast<double>(std::numeric_limits<float>::max());
            stored.polygonTwiceArea = static_cast<float>(std::clamp(
                polygonTwiceArea, -maximumArea, maximumArea));
            if (colliderPolygonBufferUse_ == colliderPolygonBuffers_.size()) {
                colliderPolygonBuffers_.emplace_back();
            }
            stored.polygonPoints = std::move(
                colliderPolygonBuffers_[colliderPolygonBufferUse_]);
            stored.polygonPoints.assign(collider.polygonPoints.begin(),
                                        collider.polygonPoints.end());
            ++colliderPolygonBufferUse_;
        }
    }
    const std::vector<MpmCollider2D>& colliders() const noexcept { return colliders_; }
    void clearColliders() {
        // Keep polygon point allocations alive while exposing only the active
        // collider vector through colliders().
        std::size_t polygonIndex = 0;
        for (auto& collider : colliders_) {
            if (collider.type != MpmCollider2D::Type::Polygon) continue;
            if (polygonIndex == colliderPolygonBuffers_.size()) {
                colliderPolygonBuffers_.emplace_back();
            }
            colliderPolygonBuffers_[polygonIndex++] =
                std::move(collider.polygonPoints);
        }
        colliders_.clear();
        colliderPolygonBufferUse_ = 0;
    }

    // ---- access ----
    int particleCount() const noexcept { return static_cast<int>(particles_.size()); }
    const MpmParticle2D& particle(int i) const noexcept { return particles_[i]; }
    const std::vector<MpmParticle2D>& particles() const noexcept { return particles_; }
    bool isGPUAttached() const noexcept { return gpuSimulation_ != nullptr; }
    bool isGPUReady() const noexcept;
    int activeCount() const noexcept;

    // ---- fracture events ----
    int fractureEventCount() const noexcept { return static_cast<int>(fracturedIndices_.size()); }
    int fractureEventIndex(int i) const noexcept { return fracturedIndices_[i]; }
    MpmVec2 fractureEventPosition(int i) const noexcept {
        int idx = fracturedIndices_[static_cast<std::size_t>(i)];
        if (idx < 0 || idx >= static_cast<int>(particles_.size())) return {0,0};
        return particles_[static_cast<std::size_t>(idx)].pos;
    }
    void clearFractureEvents();

    float cellSize() const noexcept { return cellSize_; }
    int gridNx() const noexcept { return nx_; }
    int gridNy() const noexcept { return ny_; }
    int particleGridColumns() const noexcept { return particleGridColumns_; }
    int particleGridRows() const noexcept { return particleGridRows_; }
    MpmBounds2D particleBounds(float padding = 0.0f) const noexcept;
    // Write a compact-kernel density mask using volume0 * det(F) into caller-
    // owned row-major storage. Per-particle support scales with sqrt(det(F))
    // and remains bounded to 8 output pixels per axis. For GranularDruckerPrager,
    // the supportRadius argument is ignored in favor of the configured support
    // limit; callers should use the same world extent used when configuring it.
    // The buffer contract keeps allocation and output ownership outside the
    // simulation/render path.
    bool writeDensityField(float* output, std::size_t outputCapacity,
                           int width, int height, float worldLeft,
                           float worldTop, float worldWidth, float worldHeight,
                           float supportRadius) const noexcept;
    MpmSnapshot2D snapshot() const;
    void snapshot(MpmSnapshot2D& destination) const;
    bool canRestoreSnapshot(const MpmSnapshot2D& snapshot) const noexcept;
    bool restoreSnapshot(const MpmSnapshot2D& snapshot);

    // ---- tuning ----
    void setParticlesPerCell(int ppc) { ppc_ = std::max(2, ppc); }
    int particlesPerCell() const noexcept { return ppc_; }
    float youngModulus() const noexcept { return E_; }
    float poissonRatio() const noexcept { return nu_; }
    float shearModulus() const noexcept { return mu_; }
    float lameLambda() const noexcept { return lambda_; }
    float yieldStress() const noexcept { return yieldStress_; }
    float hardening() const noexcept { return hardening_; }
    float maxPlasticStrain() const noexcept { return maxPlasticStrain_; }
    MpmVec2 gridOrigin() const noexcept { return gridOrigin_; }

private:
    bool expandGranularGridForParticles() noexcept;
    void refreshParticleBoundsCache() const noexcept;

    // grid
    float cellSize_ = 1.0f;
    MpmVec2 gridOrigin_;
    int nx_ = 32, ny_ = 32;
    int ppc_ = 4; // particles per cell per dimension
    std::vector<MpmGridNode> grid_;
    bool gridExpansionLimitReached_ = false;

    // particles
    std::vector<MpmParticle2D> particles_;
    mutable MpmBounds2D particleBoundsCache_;
    mutable bool particleBoundsCacheValid_ = false;
    std::vector<int> fracturedIndices_;
    std::vector<MpmCollider2D> colliders_;
    std::vector<std::vector<float>> colliderPolygonBuffers_;
    std::size_t colliderPolygonBufferUse_ = 0;
    int particleGridColumns_ = 0;
    int particleGridRows_ = 0;

    // material
    float E_    = 1e6f;  // Young's modulus
    float nu_   = 0.3f;  // Poisson ratio
    float mu_   = 384615.0f;   // shear modulus (E/(2(1+nu)))
    float lambda_ = 576923.0f; // Lame first parameter (E*nu/((1+nu)*(1-2*nu)))
    MpmConstitutiveModel constitutiveModel_ =
        MpmConstitutiveModel::CorotatedVonMises;
    float granularFrictionAngle_ = 0.61086524f; // 35 degrees
    float granularSupportRadiusLimit_ =
        std::numeric_limits<float>::max();

    // plasticity
    float yieldStress_ = 1e4f;
    float hardening_   = 0.1f;

    // fracture
    float maxPlasticStrain_ = 0.5f;
    float fractureEnergy_   = 0.0f;
    bool fractureEnabled_ = true;

    // damping / gravity
    float damping_   = 0.0f;
    float gravityX_  = 0.0f;
    float gravityY_  = 980.0f;  // cm/s^2

    // time
    float fixedDt_ = 1.0e-4f;
    float accumulatedTime_ = 0.0f;
    int maxSubsteps_ = 512;

    // boundary
    bool  hasBoundary_ = false;
    bool hasGranularFloor_ = false;
    float granularFloorY_ = 0.0f;
    float bxmin_ = 0.0f, bymin_ = 0.0f;
    float bxmax_ = 100.0f, bymax_ = 100.0f;
    float boundaryFriction_ = 0.0f;

    // internal helpers
    void resetGrid();
    void particleToGrid();
    void computeGridForces();
    void updateGridVelocities(float dt);
    void gridToParticle(float dt);
    void updateDeformationGradient(float dt);
    void applyPlasticity();
    void applyGranularPlasticity(MpmParticle2D& particle);
    void checkFracture();
    void applyBoundaryConditions();
    void resolveColliders();
    void updateGranularTimeStepForState() noexcept;
    void stepOnce(float dt);
    bool updateGPUSubsteps(float elapsedSeconds);

    static float weight(float x, float dx);
    static float dweight(float x, float dx);
    static MpmMat2 polarDecomposition(const MpmMat2& F);
    MpmMat2 firstPiolaKirchhoff(const MpmMat2& Fe);

    // Opaque GPU session (Graphics.MpmCompute); owned when non-null.
    void* gpuSimulation_ = nullptr;
    MpmBackend backend_ = MpmBackend::CPU;
};

} // namespace ArtifactCore
