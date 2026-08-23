module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <cstddef>

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

export struct MpmSnapshot2D {
    std::vector<MpmParticle2D> particles;
    std::vector<int> fracturedIndices;
    MpmVec2 gridOrigin;
    float accumulatedTime = 0.0f;
    int particleGridColumns = 0;
    int particleGridRows = 0;
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
    void setGrid(float cellSize, int nx, int ny);
    void setGridOrigin(float x, float y) noexcept { gridOrigin_ = {x, y}; }
    void setMaterial(float youngModulus, float poissonRatio);
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
    void setBoundaryFriction(float friction) { boundaryFriction_ = std::clamp(friction, 0.0f, 1.0f); }
    float boundaryFriction() const noexcept { return boundaryFriction_; }
    void addCollider(const MpmCollider2D& collider) { colliders_.push_back(collider); }
    const std::vector<MpmCollider2D>& colliders() const noexcept { return colliders_; }
    void clearColliders() { colliders_.clear(); }

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
    void clearFractureEvents();

    float cellSize() const noexcept { return cellSize_; }
    int gridNx() const noexcept { return nx_; }
    int gridNy() const noexcept { return ny_; }
    int particleGridColumns() const noexcept { return particleGridColumns_; }
    int particleGridRows() const noexcept { return particleGridRows_; }
    MpmSnapshot2D snapshot() const;
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
    // grid
    float cellSize_ = 1.0f;
    MpmVec2 gridOrigin_;
    int nx_ = 32, ny_ = 32;
    int ppc_ = 4; // particles per cell per dimension
    std::vector<MpmGridNode> grid_;

    // particles
    std::vector<MpmParticle2D> particles_;
    std::vector<int> fracturedIndices_;
    std::vector<MpmCollider2D> colliders_;
    int particleGridColumns_ = 0;
    int particleGridRows_ = 0;

    // material
    float E_    = 1e6f;  // Young's modulus
    float nu_   = 0.3f;  // Poisson ratio
    float mu_   = 384615.0f;   // shear modulus (E/(2(1+nu)))
    float lambda_ = 576923.0f; // Lame first parameter (E*nu/((1+nu)*(1-2*nu)))

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
    float bxmin_ = 0.0f, bymin_ = 0.0f;
    float bxmax_ = 100.0f, bymax_ = 100.0f;
    float boundaryFriction_ = 0.0f;

    // internal helpers
    void resizeGrid();
    void resetGrid();
    void particleToGrid();
    void computeGridForces();
    void updateGridVelocities(float dt);
    void gridToParticle(float dt);
    void updateDeformationGradient(float dt);
    void applyPlasticity();
    void checkFracture();
    void applyBoundaryConditions();
    void resolveColliders();
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
