module;
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

export module ArtifactCore.Particle.FluidForce;

import Particle;
import Particle.System;

namespace ArtifactCore {

/**
 * @brief Simple 3D Fluid Solver (Stable Fluids based)
 * Used as a Force Field for the Particle System.
 */
class FluidGrid {
public:
    FluidGrid(int size) : size_(size) {
        velocity_.resize(size * size * size, {0,0,0});
    }

    void addVelocity(int x, int y, int z, float3 v) {
        int idx = getIndex(x, y, z);
        if (idx >= 0) {
            velocity_[idx].x += v.x;
            velocity_[idx].y += v.y;
            velocity_[idx].z += v.z;
        }
    }

    float3 getVelocity(float x, float y, float z) const {
        // Trilinear interpolation of velocity at given world position
        int ix = static_cast<int>(x);
        int iy = static_cast<int>(y);
        int iz = static_cast<int>(z);
        int idx = getIndex(ix, iy, iz);
        if (idx < 0) return {0,0,0};
        return velocity_[idx];
    }

    void step(float dt, float viscosity) {
        ScopedPerformanceTimer timer("Fluid Simulation");
        // Simple diffusion & advection (Conceptual)
        // In a real implementation, this would involve pressure projection
        for (auto& v : velocity_) {
            v.x *= (1.0f - viscosity * dt);
            v.y *= (1.0f - viscosity * dt);
            v.z *= (1.0f - viscosity * dt);
        }
    }

private:
    int getIndex(int x, int y, int z) const {
        if (x < 0 || x >= size_ || y < 0 || y >= size_ || z < 0 || z >= size_) return -1;
        return x + y * size_ + z * size_ * size_;
    }

    int size_;
    std::vector<float3> velocity_;
};

/**
 * @brief Fluid Force Field
 * Applies air currents from a FluidGrid to particles.
 */
export class FluidForceField : public ForceField {
public:
    FluidForceField(int gridSize = 32) 
        : ForceField(Type::Turbulence), // Map to turbulence for now
          fluid_(std::make_shared<FluidGrid>(gridSize)) {}

    void apply(Particle& p, double dt) override {
        if (!enabled_) return;

        // Sample velocity from fluid grid based on particle position
        float3 v = fluid_->getVelocity(p.position.x * 0.1f, p.position.y * 0.1f, p.position.z * 0.1f);
        
        // Influence particle velocity
        p.velocity.x += v.x * influence_ * static_cast<float>(dt);
        p.velocity.y += v.y * influence_ * static_cast<float>(dt);
        p.velocity.z += v.z * influence_ * static_cast<float>(dt);
    }

    void updateFluid(float dt) {
        fluid_->step(dt, viscosity_);
    }

    void addTurbulence(float3 pos, float3 force) {
        fluid_->addVelocity(
            static_cast<int>(pos.x * 0.1f), 
            static_cast<int>(pos.y * 0.1f), 
            static_cast<int>(pos.z * 0.1f), 
            force);
    }

    void setInfluence(float influence) { influence_ = influence; }
    void setViscosity(float viscosity) { viscosity_ = viscosity; }

private:
    std::shared_ptr<FluidGrid> fluid_;
    float influence_ = 1.0f;
    float viscosity_ = 0.1f;
};

} // namespace ArtifactCore
