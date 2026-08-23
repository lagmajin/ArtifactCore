module;
#include "../Define/DllExportMacro.hpp"
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

} // namespace ArtifactCore
