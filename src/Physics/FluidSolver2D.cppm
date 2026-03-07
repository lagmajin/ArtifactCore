module;
#include <algorithm>
#include <vector>
#include <cmath>

module Physics.Fluid;

namespace ArtifactCore {

FluidSolver2D::FluidSolver2D(int width, int height) 
    : width_(width), height_(height), size_(width * height) {
    density_.resize(size_, 0.0f);
    densityPrev_.resize(size_, 0.0f);
    vx_.resize(size_, 0.0f);
    vy_.resize(size_, 0.0f);
    vxPrev_.resize(size_, 0.0f);
    vyPrev_.resize(size_, 0.0f);
    curl_.resize(size_, 0.0f);
}

FluidSolver2D::~FluidSolver2D() = default;

void FluidSolver2D::reset() {
    std::fill(density_.begin(), density_.end(), 0.0f);
    std::fill(densityPrev_.begin(), densityPrev_.end(), 0.0f);
    std::fill(vx_.begin(), vx_.end(), 0.0f);
    std::fill(vy_.begin(), vy_.end(), 0.0f);
    std::fill(vxPrev_.begin(), vxPrev_.end(), 0.0f);
    std::fill(vyPrev_.begin(), vyPrev_.end(), 0.0f);
}

void FluidSolver2D::addDensity(int x, int y, float amount) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        density_[IX(x, y)] += amount;
    }
}

void FluidSolver2D::addVelocity(int x, int y, float vx, float vy) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        vx_[IX(x, y)] += vx;
        vy_[IX(x, y)] += vy;
    }
}

float FluidSolver2D::getDensity(int x, int y) const {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        return density_[IX(x, y)];
    }
    return 0.0f;
}

void FluidSolver2D::getVelocity(int x, int y, float& vx, float& vy) const {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        vx = vx_[IX(x, y)];
        vy = vy_[IX(x, y)];
    }
}

void FluidSolver2D::setBoundary(int b, std::vector<float>& x) {
    for (int i = 1; i < width_ - 1; ++i) {
        x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
        x[IX(i, height_ - 1)] = b == 2 ? -x[IX(i, height_ - 2)] : x[IX(i, height_ - 2)];
    }
    for (int j = 1; j < height_ - 1; ++j) {
        x[IX(0, j)] = b == 1 ? -x[IX(1, j)] : x[IX(1, j)];
        x[IX(width_ - 1, j)] = b == 1 ? -x[IX(width_ - 2, j)] : x[IX(width_ - 2, j)];
    }

    x[IX(0, 0)] = 0.5f * (x[IX(1, 0)] + x[IX(0, 1)]);
    x[IX(0, height_ - 1)] = 0.5f * (x[IX(1, height_ - 1)] + x[IX(0, height_ - 2)]);
    x[IX(width_ - 1, 0)] = 0.5f * (x[IX(width_ - 2, 0)] + x[IX(width_ - 1, 1)]);
    x[IX(width_ - 1, height_ - 1)] = 0.5f * (x[IX(width_ - 2, height_ - 1)] + x[IX(width_ - 1, height_ - 2)]);
}

void FluidSolver2D::linSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c) {
    float cRecip = 1.0f / c;
    for (int k = 0; k < 20; ++k) { // 20 iterations for Gauss-Seidel
        for (int j = 1; j < height_ - 1; ++j) {
            for (int i = 1; i < width_ - 1; ++i) {
                x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i+1, j)] + x[IX(i-1, j)] + x[IX(i, j+1)] + x[IX(i, j-1)])) * cRecip;
            }
        }
        setBoundary(b, x);
    }
}

void FluidSolver2D::diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff, float dt) {
    float a = dt * diff * (width_ - 2) * (height_ - 2);
    linSolve(b, x, x0, a, 1 + 4 * a);
}

void FluidSolver2D::project(std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p, std::vector<float>& div) {
    for (int j = 1; j < height_ - 1; ++j) {
        for (int i = 1; i < width_ - 1; ++i) {
            div[IX(i, j)] = -0.5f * (vx[IX(i+1, j)] - vx[IX(i-1, j)] + vy[IX(i, j+1)] - vy[IX(i, j-1)]) / std::sqrt(width_ * height_);
            p[IX(i, j)] = 0;
        }
    }
    setBoundary(0, div);
    setBoundary(0, p);
    linSolve(0, p, div, 1, 4);

    for (int j = 1; j < height_ - 1; ++j) {
        for (int i = 1; i < width_ - 1; ++i) {
            vx[IX(i, j)] -= 0.5f * (p[IX(i+1, j)] - p[IX(i-1, j)]) * width_;
            vy[IX(i, j)] -= 0.5f * (p[IX(i, j+1)] - p[IX(i, j-1)]) * height_;
        }
    }
    setBoundary(1, vx);
    setBoundary(2, vy);
}

void FluidSolver2D::vorticityConfinement(std::vector<float>& vx, std::vector<float>& vy, float dt) {
    if (vorticityStrength_ <= 0.0f) return;

    // 1. Calculate Curl (Vorticity)
    for (int j = 1; j < height_ - 1; ++j) {
        for (int i = 1; i < width_ - 1; ++i) {
            float dv_dx = (vy[IX(i + 1, j)] - vy[IX(i - 1, j)]) * 0.5f;
            float du_dy = (vx[IX(i, j + 1)] - vx[IX(i, j - 1)]) * 0.5f;
            curl_[IX(i, j)] = std::abs(dv_dx - du_dy);
        }
    }

    // 2. Apply confinement force
    for (int j = 2; j < height_ - 2; ++j) {
        for (int i = 2; i < width_ - 2; ++i) {
            // Find gradient of the magnitude of vorticity
            float dx = (curl_[IX(i + 1, j)] - curl_[IX(i - 1, j)]) * 0.5f;
            float dy = (curl_[IX(i, j + 1)] - curl_[IX(i, j - 1)]) * 0.5f;

            float len = std::sqrt(dx * dx + dy * dy) + 1e-5f;
            dx /= len;
            dy /= len;

            float v = curl_[IX(i, j)];

            // Force is perpendicular to the gradient
            vx[IX(i, j)] += dy * v * vorticityStrength_ * dt;
            vy[IX(i, j)] -= dx * v * vorticityStrength_ * dt;
        }
    }
}

void FluidSolver2D::advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& vx, const std::vector<float>& vy, float dt) {
    float i0, i1, j0, j1;
    float dtx = dt * (width_ - 2);
    float dty = dt * (height_ - 2);
    float s0, s1, t0, t1;
    float tmp1, tmp2, x, y;

    float NfloatW = width_ - 2;
    float NfloatH = height_ - 2;

    for (int j = 1; j < height_ - 1; ++j) {
        for (int i = 1; i < width_ - 1; ++i) {
            tmp1 = dtx * vx[IX(i, j)];
            tmp2 = dty * vy[IX(i, j)];
            x = i - tmp1;
            y = j - tmp2;

            if (x < 0.5f) x = 0.5f;
            if (x > NfloatW + 0.5f) x = NfloatW + 0.5f;
            i0 = std::floor(x);
            i1 = i0 + 1.0f;
            if (y < 0.5f) y = 0.5f;
            if (y > NfloatH + 0.5f) y = NfloatH + 0.5f;
            j0 = std::floor(y);
            j1 = j0 + 1.0f;

            s1 = x - i0;
            s0 = 1.0f - s1;
            t1 = y - j0;
            t0 = 1.0f - t1;

            int i0i = static_cast<int>(i0);
            int i1i = static_cast<int>(i1);
            int j0i = static_cast<int>(j0);
            int j1i = static_cast<int>(j1);

            d[IX(i, j)] = s0 * (t0 * d0[IX(i0i, j0i)] + t1 * d0[IX(i0i, j1i)]) +
                          s1 * (t0 * d0[IX(i1i, j0i)] + t1 * d0[IX(i1i, j1i)]);
        }
    }
    setBoundary(b, d);
}

void FluidSolver2D::update(float dt) {
    // Apply Buoyancy (Thermal Convection)
    if (buoyancyFactor_ != 0.0f) {
        for (int i = 0; i < size_; ++i) {
            // Density acts as heat, creating upward velocity
            vy_[i] -= density_[i] * buoyancyFactor_ * dt;
        }
    }

    // Velocity Step
    vorticityConfinement(vx_, vy_, dt);

    diffuse(1, vxPrev_, vx_, viscosity_, dt);
    diffuse(2, vyPrev_, vy_, viscosity_, dt);
    
    project(vxPrev_, vyPrev_, vx_, vy_);
    
    advect(1, vx_, vxPrev_, vxPrev_, vyPrev_, dt);
    advect(2, vy_, vyPrev_, vxPrev_, vyPrev_, dt);
    
    project(vx_, vy_, vxPrev_, vyPrev_);
    
    // Density Step
    diffuse(0, densityPrev_, density_, diffusion_, dt);
    advect(0, density_, densityPrev_, vx_, vy_, dt);
}

} // namespace ArtifactCore
