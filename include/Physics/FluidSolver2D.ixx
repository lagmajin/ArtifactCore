module;
#include "../Define/DllExportMacro.hpp"
#include <cstddef>
#include <algorithm>
export module Physics.Fluid;

import Container.NamedVector;

export namespace ArtifactCore {

struct LIBRARY_DLL_API FluidSnapshot2D {
    int width = 0;
    int height = 0;
    float viscosity = 0.00001f;
    float diffusion = 0.00001f;
    float buoyancy = 0.05f;
    float vorticity = 0.1f;
    int solverIterations = 20;
    bool adaptiveIterations = true;
    int highResThresholdCells = 512 * 512;
    int maxAdaptiveIterations = 40;
    NamedVector<float> density{ContainerName{"Physics.FluidSnapshotDensity"}};
    NamedVector<float> densityPrev{ContainerName{"Physics.FluidSnapshotDensityPrevious"}};
    NamedVector<float> velocityX{ContainerName{"Physics.FluidSnapshotVelocityX"}};
    NamedVector<float> velocityY{ContainerName{"Physics.FluidSnapshotVelocityY"}};
    NamedVector<float> velocityXPrev{ContainerName{"Physics.FluidSnapshotVelocityXPrevious"}};
    NamedVector<float> velocityYPrev{ContainerName{"Physics.FluidSnapshotVelocityYPrevious"}};
    NamedVector<float> curl{ContainerName{"Physics.FluidSnapshotCurl"}};
};

class LIBRARY_DLL_API FluidSolver2D {
public:
    FluidSolver2D(int width, int height);
    ~FluidSolver2D();

    void update(float dt);
    FluidSnapshot2D snapshot() const;
    bool canRestoreSnapshot(const FluidSnapshot2D& snapshot) const;
    bool restoreSnapshot(const FluidSnapshot2D& snapshot);
    
    // 外部からの入力
    void addDensity(int x, int y, float amount);
    void addVelocity(int x, int y, float vx, float vy);

    // 取得
    float getDensity(int x, int y) const;
    void getVelocity(int x, int y, float& vx, float& vy) const;

    int width() const { return width_; }
    int height() const { return height_; }
    void setResolution(int width, int height);

    void setViscosity(float v) { viscosity_ = v; }
    float viscosity() const noexcept { return viscosity_; }
    void setDiffusion(float d) { diffusion_ = d; }
    float diffusion() const noexcept { return diffusion_; }
    void setBuoyancy(float b) { buoyancyFactor_ = b; }
    float buoyancy() const noexcept { return buoyancyFactor_; }
    void setVorticity(float v) { vorticityStrength_ = v; }
    float vorticity() const noexcept { return vorticityStrength_; }
    void setSolverIterations(int iterations) { solverIterations_ = std::max(1, iterations); }
    int solverIterations() const noexcept { return solverIterations_; }
    void setAdaptiveIterations(bool enabled) { adaptiveIterations_ = enabled; }
    bool adaptiveIterations() const noexcept { return adaptiveIterations_; }
    void setHighResThresholdCells(int cells) { highResThresholdCells_ = std::max(1, cells); }
    int highResThresholdCells() const noexcept { return highResThresholdCells_; }
    void setMaxAdaptiveIterations(int iterations) { maxAdaptiveIterations_ = std::max(1, iterations); }
    int maxAdaptiveIterations() const noexcept { return maxAdaptiveIterations_; }

    void reset();

private:
    int width_;
    int height_;
    int size_;

    float viscosity_ = 0.00001f;
    float diffusion_ = 0.00001f;
    float buoyancyFactor_ = 0.05f;
    float vorticityStrength_ = 0.1f;
    int solverIterations_ = 20;
    bool adaptiveIterations_ = true;
    int highResThresholdCells_ = 512 * 512;
    int maxAdaptiveIterations_ = 40;

    // Grid data
    NamedVector<float> density_{ContainerName{"Physics.FluidDensity"}};
    NamedVector<float> densityPrev_{ContainerName{"Physics.FluidDensityPrevious"}};
    
    NamedVector<float> vx_{ContainerName{"Physics.FluidVelocityX"}};
    NamedVector<float> vy_{ContainerName{"Physics.FluidVelocityY"}};
    NamedVector<float> vxPrev_{ContainerName{"Physics.FluidVelocityXPrevious"}};
    NamedVector<float> vyPrev_{ContainerName{"Physics.FluidVelocityYPrevious"}};

    // Temporary buffers for vorticity confinement
    NamedVector<float> curl_{ContainerName{"Physics.FluidCurl"}};

    // Core solvers
    void diffuse(int b, NamedVector<float>& x, const NamedVector<float>& x0, float diff, float dt);
    void advect(int b, NamedVector<float>& d, const NamedVector<float>& d0, const NamedVector<float>& vx, const NamedVector<float>& vy, float dt);
    void project(NamedVector<float>& vx, NamedVector<float>& vy, NamedVector<float>& p, NamedVector<float>& div);
    void vorticityConfinement(NamedVector<float>& vx, NamedVector<float>& vy, float dt);
    
    void setBoundary(int b, NamedVector<float>& x);
    void linSolve(int b, NamedVector<float>& x, const NamedVector<float>& x0, float a, float c);
    int computeSolverIterations() const;

    inline int IX(int x, int y) const {
        return x + y * width_;
    }
};

struct LIBRARY_DLL_API LiquidParticle2D {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float collisionImpact = 0.0f;
};

struct LIBRARY_DLL_API LiquidSnapshot2D {
    NamedVector<LiquidParticle2D> particles{
        ContainerName{"Physics.LiquidSnapshotParticles"}};
};

struct LIBRARY_DLL_API LiquidContainerPoint2D {
    float x = 0.0f;
    float y = 0.0f;
};

struct LIBRARY_DLL_API LiquidSpillParticle2D {
    float x = 0.0f;
    float y = 0.0f;
    float previousX = 0.0f;
    float previousY = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float gravityY = 0.0f;
    float size = 2.0f;
    float collisionImpact = 0.0f;
};

struct LIBRARY_DLL_API LiquidSurfaceSample2D {
    float x = 0.0f;
    float y = 0.0f;
    float size = 2.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float foamBias = 0.0f;
    float collisionImpact = 0.0f;
};

struct LIBRARY_DLL_API LiquidSurfaceTriangle2D {
    LiquidContainerPoint2D a;
    LiquidContainerPoint2D b;
    LiquidContainerPoint2D c;
    float thickness = 0.0f;
};

struct LIBRARY_DLL_API LiquidSurfaceSegment2D {
    LiquidContainerPoint2D a;
    LiquidContainerPoint2D b;
};

struct LIBRARY_DLL_API LiquidFoamPoint2D {
    LiquidContainerPoint2D position;
    float size = 1.0f;
    float alpha = 0.0f;
};

struct LIBRARY_DLL_API LiquidSurfaceSnapshot2D {
    NamedVector<LiquidSurfaceTriangle2D> triangles{
        ContainerName{"Physics.LiquidSurfaceTriangles"}};
    NamedVector<LiquidSurfaceSegment2D> contourSegments{
        ContainerName{"Physics.LiquidSurfaceContourSegments"}};
    NamedVector<LiquidFoamPoint2D> foamPoints{
        ContainerName{"Physics.LiquidSurfaceFoamPoints"}};
};

// Lightweight, deterministic 2D particle liquid for layer-local containers.
// Coordinates are normalized to the container bounds: (0,0) is top-left and
// (1,1) is bottom-right. The top edge is intentionally open.
class LIBRARY_DLL_API LiquidSolver2D {
public:
    LiquidSolver2D();
    ~LiquidSolver2D();

    void reset(float fillAmount, float particleSpacing);
    void update(float dt);
    std::size_t emitFromOpening(
        std::size_t particleCount, float normalizedWidth,
        float inwardSpeed, float normalizedPosition = 0.5f);

    void setGravity(float x, float y);
    void setViscosity(float value);
    void setSurfaceTension(float value);
    void setSubsteps(int value);
    void setSolverIterations(int value);
    bool setContainerPolygon(
        const NamedVector<LiquidContainerPoint2D>& points,
        std::size_t openEdgeIndex);
    void clearContainerPolygon();
    static void applySpillInteractions(
        NamedVector<LiquidSpillParticle2D>& particles, float dt,
        float cohesion, float viscosity);
    static LiquidSurfaceSnapshot2D buildSurfaceSnapshot(
        const NamedVector<LiquidSurfaceSample2D>& samples,
        std::size_t maximumSurfaceCells = 65536);

    float fillAmount() const noexcept { return fillAmount_; }
    float particleSpacing() const noexcept { return particleSpacing_; }
    const NamedVector<LiquidParticle2D>& particles() const noexcept {
        return particles_;
    }
    LiquidSnapshot2D snapshot() const;
    bool restore(const LiquidSnapshot2D& snapshot);
    NamedVector<LiquidParticle2D> takeEscapedParticles();

private:
    NamedVector<LiquidParticle2D> particles_{
        ContainerName{"Physics.LiquidParticles"}};
    float fillAmount_ = 0.5f;
    float particleSpacing_ = 0.055f;
    float gravityX_ = 0.0f;
    float gravityY_ = 1.8f;
    float viscosity_ = 0.08f;
    float surfaceTension_ = 0.15f;
    int substeps_ = 3;
    int solverIterations_ = 3;
    NamedVector<LiquidContainerPoint2D> containerPolygon_{
        ContainerName{"Physics.LiquidContainerPolygon"}};
    std::size_t openContainerEdge_ = 0;

    void solveParticleDistances();
    void solveContainerBounds(LiquidParticle2D& particle) const;
    bool escapedThroughContainerOpening(
        const LiquidParticle2D& particle) const;
    void applyViscosity();
};

} // namespace ArtifactCore
