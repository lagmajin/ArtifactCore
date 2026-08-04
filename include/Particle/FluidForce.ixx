module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

export module ArtifactCore.Particle.FluidForce;

import Memory.SharedPtr;
import Particle;
import Particle.System;

namespace ArtifactCore {

/**
 * @brief Simple 3D Fluid Solver (Stable Fluids based)
 * Used as a Force Field for the Particle System.
 */
class FluidGrid {
public:
    FluidGrid(int size) : size_(std::clamp(size, 2, 256)) {
        velocity_.resize(size_ * size_ * size_, {0,0,0});
    }

    void addVelocity(int x, int y, int z, float3 v) {
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return;
        int idx = getIndex(x, y, z);
        if (idx >= 0) {
            velocity_[idx].x += v.x;
            velocity_[idx].y += v.y;
            velocity_[idx].z += v.z;
        }
    }

    float3 getVelocity(float x, float y, float z) const {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return {0,0,0};
        const float cx = std::clamp(x, 0.0f, static_cast<float>(size_ - 1));
        const float cy = std::clamp(y, 0.0f, static_cast<float>(size_ - 1));
        const float cz = std::clamp(z, 0.0f, static_cast<float>(size_ - 1));
        const int x0 = static_cast<int>(std::floor(cx));
        const int y0 = static_cast<int>(std::floor(cy));
        const int z0 = static_cast<int>(std::floor(cz));
        const int x1 = std::min(x0 + 1, size_ - 1);
        const int y1 = std::min(y0 + 1, size_ - 1);
        const int z1 = std::min(z0 + 1, size_ - 1);
        const float tx = cx - x0;
        const float ty = cy - y0;
        const float tz = cz - z0;
        const auto sample = [this](int sx, int sy, int sz) {
            return velocity_[getIndex(sx, sy, sz)];
        };
        const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        const float3 c000 = sample(x0, y0, z0);
        const float3 c100 = sample(x1, y0, z0);
        const float3 c010 = sample(x0, y1, z0);
        const float3 c110 = sample(x1, y1, z0);
        const float3 c001 = sample(x0, y0, z1);
        const float3 c101 = sample(x1, y0, z1);
        const float3 c011 = sample(x0, y1, z1);
        const float3 c111 = sample(x1, y1, z1);
        return {
            lerp(lerp(lerp(c000.x, c100.x, tx), lerp(c010.x, c110.x, tx), ty),
                 lerp(lerp(c001.x, c101.x, tx), lerp(c011.x, c111.x, tx), ty), tz),
            lerp(lerp(lerp(c000.y, c100.y, tx), lerp(c010.y, c110.y, tx), ty),
                 lerp(lerp(c001.y, c101.y, tx), lerp(c011.y, c111.y, tx), ty), tz),
            lerp(lerp(lerp(c000.z, c100.z, tx), lerp(c010.z, c110.z, tx), ty),
                 lerp(lerp(c001.z, c101.z, tx), lerp(c011.z, c111.z, tx), ty), tz)
        };
    }

    void step(float dt, float viscosity) {
        if (!std::isfinite(dt) || !std::isfinite(viscosity) || dt <= 0.0f) return;
        const float damping = std::clamp(1.0f - std::max(0.0f, viscosity) * dt, 0.0f, 1.0f);
        const auto previous = velocity_;
        for (int z = 0; z < size_; ++z) {
            for (int y = 0; y < size_; ++y) {
                for (int x = 0; x < size_; ++x) {
                    const float3 advecting = previous[x + y * size_ + z * size_ * size_];
                    const int sourceX = std::clamp(
                        static_cast<int>(std::lround(x - advecting.x * dt)), 0, size_ - 1);
                    const int sourceY = std::clamp(
                        static_cast<int>(std::lround(y - advecting.y * dt)), 0, size_ - 1);
                    const int sourceZ = std::clamp(
                        static_cast<int>(std::lround(z - advecting.z * dt)), 0, size_ - 1);
                    auto& v = velocity_[x + y * size_ + z * size_ * size_];
                    const auto& source = previous[sourceX + sourceY * size_ +
                                                  sourceZ * size_ * size_];
                    v.x = source.x * damping;
                    v.y = source.y * damping;
                    v.z = source.z * damping;
                }
            }
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
        : ForceField(Type::Fluid),
          fluid_(makeShared<FluidGrid>(gridSize)) {}

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

    void setInfluence(float influence) { influence_ = std::isfinite(influence) ? influence : 0.0f; }
    void setViscosity(float viscosity) { viscosity_ = std::isfinite(viscosity) ? std::max(0.0f, viscosity) : 0.0f; }

private:
    SharedPtr<FluidGrid> fluid_;
    float influence_ = 1.0f;
    float viscosity_ = 0.1f;
};

} // namespace ArtifactCore
