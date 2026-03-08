module;
#include <algorithm>
#include <vector>
#include <cmath>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

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

int FluidSolver2D::computeSolverIterations() const {
    if (!adaptiveIterations_) {
        return solverIterations_;
    }

    if (size_ < highResThresholdCells_) {
        return solverIterations_;
    }

    const int extra = std::max(1, size_ / highResThresholdCells_);
    return std::min(maxAdaptiveIterations_, solverIterations_ + extra * 4);
}

bool FluidSolver2D::useParallelPath() const {
    return parallelEnabled_ && size_ >= parallelThresholdCells_;
}

void FluidSolver2D::linSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c) {
    float cRecip = 1.0f / c;
    const int iterations = computeSolverIterations();

    if (useParallelPath()) {
        // Red-black Gauss-Seidel keeps convergence behavior while enabling safe row-parallel updates.
        for (int k = 0; k < iterations; ++k) {
            for (int parity = 0; parity < 2; ++parity) {
                tbb::parallel_for(tbb::blocked_range<int>(1, height_ - 1),
                    [&](const tbb::blocked_range<int>& rows) {
                        for (int j = rows.begin(); j < rows.end(); ++j) {
                            int iStart = 1 + ((j + parity) & 1);
                            for (int i = iStart; i < width_ - 1; i += 2) {
                                x[IX(i, j)] = (x0[IX(i, j)] +
                                    a * (x[IX(i + 1, j)] + x[IX(i - 1, j)] + x[IX(i, j + 1)] + x[IX(i, j - 1)])) * cRecip;
                            }
                        }
                    });
            }
            setBoundary(b, x);
        }
        return;
    }

    for (int k = 0; k < iterations; ++k) {
        for (int j = 1; j < height_ - 1; ++j) {
            for (int i = 1; i < width_ - 1; ++i) {
                x[IX(i, j)] = (x0[IX(i, j)] +
                    a * (x[IX(i + 1, j)] + x[IX(i - 1, j)] + x[IX(i, j + 1)] + x[IX(i, j - 1)])) * cRecip;
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
    const float invScale = 1.0f / std::sqrt(static_cast<float>(width_ * height_));
    if (useParallelPath()) {
        tbb::parallel_for(tbb::blocked_range<int>(1, height_ - 1),
            [&](const tbb::blocked_range<int>& rows) {
                for (int j = rows.begin(); j < rows.end(); ++j) {
                    for (int i = 1; i < width_ - 1; ++i) {
                        div[IX(i, j)] = -0.5f * (vx[IX(i + 1, j)] - vx[IX(i - 1, j)] + vy[IX(i, j + 1)] - vy[IX(i, j - 1)]) * invScale;
                        p[IX(i, j)] = 0.0f;
                    }
                }
            });
    } else {
        for (int j = 1; j < height_ - 1; ++j) {
            for (int i = 1; i < width_ - 1; ++i) {
                div[IX(i, j)] = -0.5f * (vx[IX(i + 1, j)] - vx[IX(i - 1, j)] + vy[IX(i, j + 1)] - vy[IX(i, j - 1)]) * invScale;
                p[IX(i, j)] = 0.0f;
            }
        }
    }
    setBoundary(0, div);
    setBoundary(0, p);
    linSolve(0, p, div, 1, 4);

    if (useParallelPath()) {
        tbb::parallel_for(tbb::blocked_range<int>(1, height_ - 1),
            [&](const tbb::blocked_range<int>& rows) {
                for (int j = rows.begin(); j < rows.end(); ++j) {
                    for (int i = 1; i < width_ - 1; ++i) {
                        vx[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * width_;
                        vy[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * height_;
                    }
                }
            });
    } else {
        for (int j = 1; j < height_ - 1; ++j) {
            for (int i = 1; i < width_ - 1; ++i) {
                vx[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * width_;
                vy[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * height_;
            }
        }
    }
    setBoundary(1, vx);
    setBoundary(2, vy);
}

void FluidSolver2D::vorticityConfinement(std::vector<float>& vx, std::vector<float>& vy, float dt) {
    if (vorticityStrength_ <= 0.0f) return;

    // 1. Calculate Curl (Vorticity)
    if (useParallelPath()) {
        tbb::parallel_for(tbb::blocked_range<int>(1, height_ - 1),
            [&](const tbb::blocked_range<int>& rows) {
                for (int j = rows.begin(); j < rows.end(); ++j) {
                    for (int i = 1; i < width_ - 1; ++i) {
                        float dv_dx = (vy[IX(i + 1, j)] - vy[IX(i - 1, j)]) * 0.5f;
                        float du_dy = (vx[IX(i, j + 1)] - vx[IX(i, j - 1)]) * 0.5f;
                        curl_[IX(i, j)] = std::abs(dv_dx - du_dy);
                    }
                }
            });
    } else {
        for (int j = 1; j < height_ - 1; ++j) {
            for (int i = 1; i < width_ - 1; ++i) {
                float dv_dx = (vy[IX(i + 1, j)] - vy[IX(i - 1, j)]) * 0.5f;
                float du_dy = (vx[IX(i, j + 1)] - vx[IX(i, j - 1)]) * 0.5f;
                curl_[IX(i, j)] = std::abs(dv_dx - du_dy);
            }
        }
    }

    // 2. Apply confinement force
    if (useParallelPath()) {
        tbb::parallel_for(tbb::blocked_range<int>(2, height_ - 2),
            [&](const tbb::blocked_range<int>& rows) {
                for (int j = rows.begin(); j < rows.end(); ++j) {
                    for (int i = 2; i < width_ - 2; ++i) {
                        float dx = (curl_[IX(i + 1, j)] - curl_[IX(i - 1, j)]) * 0.5f;
                        float dy = (curl_[IX(i, j + 1)] - curl_[IX(i, j - 1)]) * 0.5f;
                        float len = std::sqrt(dx * dx + dy * dy) + 1e-5f;
                        dx /= len;
                        dy /= len;
                        float v = curl_[IX(i, j)];
                        vx[IX(i, j)] += dy * v * vorticityStrength_ * dt;
                        vy[IX(i, j)] -= dx * v * vorticityStrength_ * dt;
                    }
                }
            });
    } else {
        for (int j = 2; j < height_ - 2; ++j) {
            for (int i = 2; i < width_ - 2; ++i) {
                float dx = (curl_[IX(i + 1, j)] - curl_[IX(i - 1, j)]) * 0.5f;
                float dy = (curl_[IX(i, j + 1)] - curl_[IX(i, j - 1)]) * 0.5f;
                float len = std::sqrt(dx * dx + dy * dy) + 1e-5f;
                dx /= len;
                dy /= len;
                float v = curl_[IX(i, j)];
                vx[IX(i, j)] += dy * v * vorticityStrength_ * dt;
                vy[IX(i, j)] -= dx * v * vorticityStrength_ * dt;
            }
        }
    }
}

void FluidSolver2D::advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& vx, const std::vector<float>& vy, float dt) {
    float dtx = dt * (width_ - 2);
    float dty = dt * (height_ - 2);

    float NfloatW = width_ - 2;
    float NfloatH = height_ - 2;

    const auto advectRows = [&](int rowBegin, int rowEnd) {
        for (int j = rowBegin; j < rowEnd; ++j) {
            for (int i = 1; i < width_ - 1; ++i) {
                float tmp1 = dtx * vx[IX(i, j)];
                float tmp2 = dty * vy[IX(i, j)];
                float x = i - tmp1;
                float y = j - tmp2;

                if (x < 0.5f) x = 0.5f;
                if (x > NfloatW + 0.5f) x = NfloatW + 0.5f;
                float i0 = std::floor(x);
                float i1 = i0 + 1.0f;
                if (y < 0.5f) y = 0.5f;
                if (y > NfloatH + 0.5f) y = NfloatH + 0.5f;
                float j0 = std::floor(y);
                float j1 = j0 + 1.0f;

                float s1 = x - i0;
                float s0 = 1.0f - s1;
                float t1 = y - j0;
                float t0 = 1.0f - t1;

                int i0i = static_cast<int>(i0);
                int i1i = static_cast<int>(i1);
                int j0i = static_cast<int>(j0);
                int j1i = static_cast<int>(j1);

                d[IX(i, j)] = s0 * (t0 * d0[IX(i0i, j0i)] + t1 * d0[IX(i0i, j1i)]) +
                              s1 * (t0 * d0[IX(i1i, j0i)] + t1 * d0[IX(i1i, j1i)]);
            }
        }
    };

    if (useParallelPath()) {
        tbb::parallel_for(tbb::blocked_range<int>(1, height_ - 1),
            [&](const tbb::blocked_range<int>& rows) {
                advectRows(rows.begin(), rows.end());
            });
    } else {
        advectRows(1, height_ - 1);
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
