module;
#include "../Define/DllExportMacro.hpp"
#include <cstddef>
#include <vector>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Physics.Fluid;

export namespace ArtifactCore {

class LIBRARY_DLL_API FluidSolver2D {
public:
    FluidSolver2D(int width, int height);
    ~FluidSolver2D();

    void update(float dt);
    
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
    std::vector<float> density_;
    std::vector<float> densityPrev_;
    
    std::vector<float> vx_;
    std::vector<float> vy_;
    std::vector<float> vxPrev_;
    std::vector<float> vyPrev_;

    // Temporary buffers for vorticity confinement
    std::vector<float> curl_;

    // Core solvers
    void diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff, float dt);
    void advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& vx, const std::vector<float>& vy, float dt);
    void project(std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p, std::vector<float>& div);
    void vorticityConfinement(std::vector<float>& vx, std::vector<float>& vy, float dt);
    
    void setBoundary(int b, std::vector<float>& x);
    void linSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c);
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
    std::vector<LiquidParticle2D> particles;
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
    std::vector<LiquidSurfaceTriangle2D> triangles;
    std::vector<LiquidSurfaceSegment2D> contourSegments;
    std::vector<LiquidFoamPoint2D> foamPoints;
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
        const std::vector<LiquidContainerPoint2D>& points,
        std::size_t openEdgeIndex);
    void clearContainerPolygon();
    static void applySpillInteractions(
        std::vector<LiquidSpillParticle2D>& particles, float dt,
        float cohesion, float viscosity);
    static LiquidSurfaceSnapshot2D buildSurfaceSnapshot(
        const std::vector<LiquidSurfaceSample2D>& samples,
        std::size_t maximumSurfaceCells = 65536);

    float fillAmount() const noexcept { return fillAmount_; }
    float particleSpacing() const noexcept { return particleSpacing_; }
    const std::vector<LiquidParticle2D>& particles() const noexcept {
        return particles_;
    }
    LiquidSnapshot2D snapshot() const;
    bool restore(const LiquidSnapshot2D& snapshot);
    std::vector<LiquidParticle2D> takeEscapedParticles();

private:
    std::vector<LiquidParticle2D> particles_;
    float fillAmount_ = 0.5f;
    float particleSpacing_ = 0.055f;
    float gravityX_ = 0.0f;
    float gravityY_ = 1.8f;
    float viscosity_ = 0.08f;
    float surfaceTension_ = 0.15f;
    int substeps_ = 3;
    int solverIterations_ = 3;
    std::vector<LiquidContainerPoint2D> containerPolygon_;
    std::size_t openContainerEdge_ = 0;

    void solveParticleDistances();
    void solveContainerBounds(LiquidParticle2D& particle) const;
    bool escapedThroughContainerOpening(
        const LiquidParticle2D& particle) const;
    void applyViscosity();
};

} // namespace ArtifactCore
