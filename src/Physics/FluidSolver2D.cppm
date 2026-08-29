module;
#include <utility>
#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>
#include <map>
#include <set>

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

void FluidSolver2D::setResolution(int width, int height) {
    const int newWidth = std::max(4, width);
    const int newHeight = std::max(4, height);
    if (newWidth == width_ && newHeight == height_) return;
    const int oldWidth = width_;
    const int oldHeight = height_;
    const auto oldDensity = density_;
    const auto oldVx = vx_;
    const auto oldVy = vy_;
    width_ = newWidth;
    height_ = newHeight;
    size_ = width_ * height_;
    density_.assign(size_, 0.0f);
    densityPrev_.assign(size_, 0.0f);
    vx_.assign(size_, 0.0f);
    vy_.assign(size_, 0.0f);
    vxPrev_.assign(size_, 0.0f);
    vyPrev_.assign(size_, 0.0f);
    curl_.assign(size_, 0.0f);
    const auto sample = [oldWidth, oldHeight](const std::vector<float>& field, float x, float y) {
        const int ix = std::clamp(static_cast<int>(std::lround(x)), 0, oldWidth - 1);
        const int iy = std::clamp(static_cast<int>(std::lround(y)), 0, oldHeight - 1);
        return field[ix + iy * oldWidth];
    };
    for (int y = 0; y < height_; ++y) {
        const float sourceY = static_cast<float>(y) * static_cast<float>(oldHeight - 1) /
                              static_cast<float>(height_ - 1);
        for (int x = 0; x < width_; ++x) {
            const float sourceX = static_cast<float>(x) * static_cast<float>(oldWidth - 1) /
                                  static_cast<float>(width_ - 1);
            const int index = IX(x, y);
            density_[index] = sample(oldDensity, sourceX, sourceY);
            vx_[index] = sample(oldVx, sourceX, sourceY);
            vy_[index] = sample(oldVy, sourceX, sourceY);
        }
    }
}

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

    // Smooth step at threshold: no jump at size == threshold,
    // +4 per additional threshold block. Previously max(1, size/threshold)
    // caused an immediate +4 at the threshold boundary.
    const int blocks = size_ / highResThresholdCells_;
    const int extra = std::max(0, blocks - 1);
    return std::min(maxAdaptiveIterations_, solverIterations_ + extra * 4);
}

void FluidSolver2D::linSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c) {
    float cRecip = 1.0f / c;
    const int iterations = computeSolverIterations();

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
    for (int j = 1; j < height_ - 1; ++j) {
        for (int i = 1; i < width_ - 1; ++i) {
            div[IX(i, j)] = -0.5f * (vx[IX(i + 1, j)] - vx[IX(i - 1, j)] + vy[IX(i, j + 1)] - vy[IX(i, j - 1)]) * invScale;
            p[IX(i, j)] = 0.0f;
        }
    }
    setBoundary(0, div);
    setBoundary(0, p);
    linSolve(0, p, div, 1, 4);

    for (int j = 1; j < height_ - 1; ++j) {
        for (int i = 1; i < width_ - 1; ++i) {
            vx[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * width_;
            vy[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * height_;
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

void FluidSolver2D::advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& vx, const std::vector<float>& vy, float dt) {
    float dtx = dt * (width_ - 2);
    float dty = dt * (height_ - 2);

    float NfloatW = width_ - 2;
    float NfloatH = height_ - 2;

    for (int j = 1; j < height_ - 1; ++j) {
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

    setBoundary(b, d);
}

void FluidSolver2D::update(float dt) {
    // Guard against zero/negative or exploding frame deltas so the
    // semi-Lagrangian advection and pressure solve stay stable.
    if (dt <= 0.0f) return;
    dt = std::min(dt, 0.05f);

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

    // Divergence guard: replace non-finite samples with neutral values so
    // one bad advection step cannot poison subsequent solves.
    for (int i = 0; i < size_; ++i) {
        if (!std::isfinite(density_[i])) density_[i] = 0.0f;
        if (!std::isfinite(vx_[i])) vx_[i] = 0.0f;
        if (!std::isfinite(vy_[i])) vy_[i] = 0.0f;
    }
}

LiquidSolver2D::LiquidSolver2D() = default;
LiquidSolver2D::~LiquidSolver2D() = default;

namespace {
bool liquidPointInsidePolygon(
    const LiquidContainerPoint2D& point,
    const std::vector<LiquidContainerPoint2D>& polygon) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size();
         j = i++) {
        const auto& a = polygon[i];
        const auto& b = polygon[j];
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) /
                               ((b.y - a.y) + 1.0e-12f) + a.x);
        if (crosses) inside = !inside;
    }
    return inside;
}

LiquidContainerPoint2D liquidClosestPointOnSegment(
    const LiquidContainerPoint2D& point,
    const LiquidContainerPoint2D& a,
    const LiquidContainerPoint2D& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1.0e-12f) return a;
    const float t = std::clamp(
        ((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared,
        0.0f, 1.0f);
    return {a.x + dx * t, a.y + dy * t};
}

template <typename PairVisitor>
void forEachLiquidNeighborPair(
    const std::vector<LiquidParticle2D>& particles,
    float cellSize,
    PairVisitor&& visitor) {
    if (particles.size() < 2 || !std::isfinite(cellSize) || cellSize <= 0.0f) {
        return;
    }

    using Cell = std::pair<int, int>;
    std::map<Cell, std::vector<std::size_t>> grid;
    std::vector<Cell> particleCells(particles.size());
    std::vector<bool> valid(particles.size(), false);
    const float inverseCellSize = 1.0f / cellSize;
    const auto cellCoordinate = [inverseCellSize](float value) {
        const double scaled = std::floor(
            static_cast<double>(value) * static_cast<double>(inverseCellSize));
        return static_cast<int>(std::clamp(
            scaled,
            static_cast<double>(std::numeric_limits<int>::min()),
            static_cast<double>(std::numeric_limits<int>::max())));
    };
    const auto adjacentCoordinate = [](int value, int offset) {
        const long long adjacent = static_cast<long long>(value) + offset;
        return static_cast<int>(std::clamp(
            adjacent,
            static_cast<long long>(std::numeric_limits<int>::min()),
            static_cast<long long>(std::numeric_limits<int>::max())));
    };
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const auto& particle = particles[i];
        if (!std::isfinite(particle.x) || !std::isfinite(particle.y)) continue;
        const Cell cell{cellCoordinate(particle.x), cellCoordinate(particle.y)};
        particleCells[i] = cell;
        valid[i] = true;
        grid[cell].push_back(i);
    }

    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        if (!valid[i]) continue;
        candidates.clear();
        const auto [cellX, cellY] = particleCells[i];
        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const auto bucket = grid.find({
                    adjacentCoordinate(cellX, offsetX),
                    adjacentCoordinate(cellY, offsetY)});
                if (bucket == grid.end()) continue;
                for (const std::size_t j : bucket->second) {
                    if (j > i) candidates.push_back(j);
                }
            }
        }
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(
            std::unique(candidates.begin(), candidates.end()), candidates.end());
        for (const std::size_t j : candidates) visitor(i, j);
    }
}
} // namespace

bool LiquidSolver2D::setContainerPolygon(
    const std::vector<LiquidContainerPoint2D>& points,
    std::size_t openEdgeIndex) {
    constexpr std::size_t maxContainerPoints = 512;
    if (points.size() < 3 || points.size() > maxContainerPoints) return false;

    std::vector<LiquidContainerPoint2D> sanitized;
    sanitized.reserve(points.size());
    for (const auto& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
        if (!sanitized.empty()) {
            const float dx = point.x - sanitized.back().x;
            const float dy = point.y - sanitized.back().y;
            if (dx * dx + dy * dy <= 1.0e-12f) continue;
        }
        sanitized.push_back(point);
    }
    if (sanitized.size() >= 2) {
        const float dx = sanitized.front().x - sanitized.back().x;
        const float dy = sanitized.front().y - sanitized.back().y;
        if (dx * dx + dy * dy <= 1.0e-12f) sanitized.pop_back();
    }
    if (sanitized.size() < 3) return false;

    double twiceArea = 0.0;
    for (std::size_t i = 0; i < sanitized.size(); ++i) {
        const auto& a = sanitized[i];
        const auto& b = sanitized[(i + 1) % sanitized.size()];
        twiceArea += static_cast<double>(a.x) * b.y -
                     static_cast<double>(b.x) * a.y;
    }
    if (std::abs(twiceArea) <= 1.0e-8) return false;
    containerPolygon_ = std::move(sanitized);
    openContainerEdge_ = std::min(openEdgeIndex, containerPolygon_.size() - 1);
    return true;
}

void LiquidSolver2D::clearContainerPolygon() {
    containerPolygon_.clear();
    openContainerEdge_ = 0;
}

std::size_t LiquidSolver2D::emitFromOpening(
    std::size_t particleCount, float normalizedWidth, float inwardSpeed,
    float normalizedPosition) {
    constexpr std::size_t maxParticles = 100000;
    constexpr std::size_t maxEmissionBatch = 4096;
    if (particleCount == 0 || particles_.size() >= maxParticles) return 0;
    if (!std::isfinite(normalizedWidth) || !std::isfinite(inwardSpeed) ||
        !std::isfinite(normalizedPosition)) {
        return 0;
    }
    particleCount = std::min(
        {particleCount, maxEmissionBatch, maxParticles - particles_.size()});
    normalizedWidth = std::clamp(normalizedWidth, 0.0f, 1.0f);
    inwardSpeed = std::clamp(inwardSpeed, 0.0f, 20.0f);
    normalizedPosition = std::clamp(normalizedPosition, 0.0f, 1.0f);

    LiquidContainerPoint2D openingA{0.0f, 0.0f};
    LiquidContainerPoint2D openingB{1.0f, 0.0f};
    if (!containerPolygon_.empty()) {
        openingA = containerPolygon_[openContainerEdge_];
        openingB = containerPolygon_[
            (openContainerEdge_ + 1) % containerPolygon_.size()];
    }
    float tangentX = openingB.x - openingA.x;
    float tangentY = openingB.y - openingA.y;
    const float openingLength = std::hypot(tangentX, tangentY);
    if (!std::isfinite(openingLength) || openingLength <= 1.0e-6f) return 0;
    tangentX /= openingLength;
    tangentY /= openingLength;

    const float edgeMargin = std::min(
        0.49f, particleSpacing_ * 1.5f / openingLength);
    const float safePosition = std::clamp(
        normalizedPosition, edgeMargin, 1.0f - edgeMargin);
    const LiquidContainerPoint2D sourceCenter{
        openingA.x + tangentX * openingLength * safePosition,
        openingA.y + tangentY * openingLength * safePosition};
    float inwardX = -tangentY;
    float inwardY = tangentX;
    const float probeDistance = std::max(particleSpacing_ * 1.5f, 0.01f);
    if (!containerPolygon_.empty()) {
        const LiquidContainerPoint2D positiveProbe{
            sourceCenter.x + inwardX * probeDistance,
            sourceCenter.y + inwardY * probeDistance};
        const LiquidContainerPoint2D negativeProbe{
            sourceCenter.x - inwardX * probeDistance,
            sourceCenter.y - inwardY * probeDistance};
        const bool positiveInside =
            liquidPointInsidePolygon(positiveProbe, containerPolygon_);
        const bool negativeInside =
            liquidPointInsidePolygon(negativeProbe, containerPolygon_);
        if (!positiveInside && negativeInside) {
            inwardX = -inwardX;
            inwardY = -inwardY;
        } else if (!positiveInside && !negativeInside) {
            return 0;
        }
    } else {
        inwardX = 0.0f;
        inwardY = 1.0f;
    }

    const float usableWidth = std::max(
        0.0f, openingLength * normalizedWidth - particleSpacing_ * 2.0f);
    const double availableLaneCount = std::clamp(
        std::floor(static_cast<double>(usableWidth) / particleSpacing_) + 1.0,
        1.0, static_cast<double>(maxEmissionBatch));
    const std::size_t availableLanes =
        static_cast<std::size_t>(availableLaneCount);
    const std::size_t laneCount = std::min(particleCount, availableLanes);
    const float occupiedDistance = particleSpacing_ * 0.72f;
    const float occupiedDistanceSquared = occupiedDistance * occupiedDistance;
    const float occupancyCellSize = std::max(occupiedDistance, 1.0e-4f);
    const float inverseOccupancyCellSize = 1.0f / occupancyCellSize;
    const auto occupancyCell = [inverseOccupancyCellSize](float value) {
        const double scaled = std::floor(
            static_cast<double>(value) * inverseOccupancyCellSize);
        return static_cast<int>(std::clamp(
            scaled,
            static_cast<double>(std::numeric_limits<int>::min()),
            static_cast<double>(std::numeric_limits<int>::max())));
    };
    const auto adjacentCell = [](int value, int offset) {
        const long long adjacent = static_cast<long long>(value) + offset;
        return static_cast<int>(std::clamp(
            adjacent,
            static_cast<long long>(std::numeric_limits<int>::min()),
            static_cast<long long>(std::numeric_limits<int>::max())));
    };
    using OccupancyCell = std::pair<int, int>;
    std::map<OccupancyCell, std::vector<std::size_t>> occupancy;
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        const auto& particle = particles_[i];
        if (!std::isfinite(particle.x) || !std::isfinite(particle.y)) continue;
        occupancy[{occupancyCell(particle.x), occupancyCell(particle.y)}]
            .push_back(i);
    }

    std::size_t emitted = 0;
    const float sourceDistanceFromStart = openingLength * safePosition;
    const float minimumLane =
        particleSpacing_ - sourceDistanceFromStart;
    const float maximumLane =
        openingLength - particleSpacing_ - sourceDistanceFromStart;
    for (std::size_t i = 0; i < laneCount; ++i) {
        const float lane =
            (static_cast<float>(i) -
             static_cast<float>(laneCount - 1) * 0.5f) * particleSpacing_;
        if (lane < minimumLane || lane > maximumLane) continue;
        LiquidParticle2D particle;
        particle.x =
            sourceCenter.x + tangentX * lane + inwardX * probeDistance;
        particle.y =
            sourceCenter.y + tangentY * lane + inwardY * probeDistance;
        particle.vx = inwardX * inwardSpeed;
        particle.vy = inwardY * inwardSpeed;
        if (!containerPolygon_.empty() &&
            !liquidPointInsidePolygon({particle.x, particle.y},
                                      containerPolygon_)) {
            continue;
        }
        const int cellX = occupancyCell(particle.x);
        const int cellY = occupancyCell(particle.y);
        bool occupied = false;
        for (int offsetY = -1; offsetY <= 1 && !occupied; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1 && !occupied; ++offsetX) {
                const auto bucket = occupancy.find(
                    {adjacentCell(cellX, offsetX),
                     adjacentCell(cellY, offsetY)});
                if (bucket == occupancy.end()) continue;
                for (const std::size_t existingIndex : bucket->second) {
                    const auto& existing = particles_[existingIndex];
                    const float dx = particle.x - existing.x;
                    const float dy = particle.y - existing.y;
                    if (dx * dx + dy * dy < occupiedDistanceSquared) {
                        occupied = true;
                        break;
                    }
                }
            }
        }
        if (occupied) continue;
        const std::size_t emittedIndex = particles_.size();
        particles_.push_back(particle);
        occupancy[{cellX, cellY}].push_back(emittedIndex);
        ++emitted;
    }
    return emitted;
}

void LiquidSolver2D::applySpillInteractions(
    std::vector<LiquidSpillParticle2D>& particles, float dt,
    float cohesion, float viscosity) {
    if (particles.size() < 2 || !std::isfinite(dt) || dt <= 0.0f) return;
    dt = std::min(dt, 0.05f);
    cohesion = std::clamp(cohesion, 0.0f, 1.0f);
    viscosity = std::clamp(viscosity, 0.0f, 1.0f);

    float maximumSize = 2.0f;
    for (const auto& particle : particles) {
        if (std::isfinite(particle.size)) {
            maximumSize = std::max(maximumSize, particle.size);
        }
    }
    const float cellSize = std::max(2.0f, maximumSize * 2.25f);
    std::map<std::pair<int, int>, std::vector<std::size_t>> grid;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const auto& particle = particles[i];
        if (!std::isfinite(particle.x) || !std::isfinite(particle.y)) continue;
        const int cellX = static_cast<int>(std::floor(particle.x / cellSize));
        const int cellY = static_cast<int>(std::floor(particle.y / cellSize));
        grid[{cellX, cellY}].push_back(i);
    }

    std::vector<std::pair<float, float>> velocityDelta(
        particles.size(), {0.0f, 0.0f});
    const float viscosityBlend = 1.0f - std::exp(-viscosity * 7.67f * dt);
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const auto& a = particles[i];
        if (!std::isfinite(a.x) || !std::isfinite(a.y)) continue;
        const int cellX = static_cast<int>(std::floor(a.x / cellSize));
        const int cellY = static_cast<int>(std::floor(a.y / cellSize));
        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                const auto bucket = grid.find(
                    {cellX + offsetX, cellY + offsetY});
                if (bucket == grid.end()) continue;
                for (const std::size_t j : bucket->second) {
                    if (j <= i) continue;
                    const auto& b = particles[j];
                    const float dx = b.x - a.x;
                    const float dy = b.y - a.y;
                    const float distanceSquared = dx * dx + dy * dy;
                    const float pairSize =
                        std::max(0.1f, std::max(a.size, b.size));
                    const float interactionRadius = pairSize * 2.25f;
                    if (!std::isfinite(distanceSquared) ||
                        distanceSquared >= interactionRadius * interactionRadius) {
                        continue;
                    }
                    const float distance =
                        std::sqrt(std::max(distanceSquared, 1.0e-8f));
                    float normalX = dx / distance;
                    float normalY = dy / distance;
                    if (distanceSquared <= 1.0e-8f) {
                        normalX = ((i + j) & 1U) ? 1.0f : -1.0f;
                        normalY = 0.0f;
                    }
                    const float restDistance = pairSize * 0.72f;
                    const float attraction = cohesion *
                        std::max(0.0f, distance - restDistance) /
                        interactionRadius * interactionRadius * 18.0f;
                    const float separation =
                        distance < restDistance
                            ? (restDistance - distance) /
                                  std::max(restDistance, 1.0e-4f) *
                                  interactionRadius * 34.0f
                            : 0.0f;
                    const float pairVelocityDelta =
                        (attraction - separation) * dt;
                    float deltaX = normalX * pairVelocityDelta +
                        (b.vx - a.vx) * viscosityBlend * 0.5f;
                    float deltaY = normalY * pairVelocityDelta +
                        (b.vy - a.vy) * viscosityBlend * 0.5f;
                    velocityDelta[i].first += deltaX;
                    velocityDelta[i].second += deltaY;
                    velocityDelta[j].first -= deltaX;
                    velocityDelta[j].second -= deltaY;
                }
            }
        }
    }

    for (std::size_t i = 0; i < particles.size(); ++i) {
        float deltaX = velocityDelta[i].first;
        float deltaY = velocityDelta[i].second;
        const float deltaLength = std::hypot(deltaX, deltaY);
        const float maximumDelta =
            std::max(4.0f, std::max(0.1f, particles[i].size) * 2.0f);
        if (deltaLength > maximumDelta) {
            const float scale = maximumDelta / deltaLength;
            deltaX *= scale;
            deltaY *= scale;
        }
        if (std::isfinite(deltaX) && std::isfinite(deltaY)) {
            particles[i].vx += deltaX;
            particles[i].vy += deltaY;
        }
    }
}

LiquidSurfaceSnapshot2D LiquidSolver2D::buildSurfaceSnapshot(
    const std::vector<LiquidSurfaceSample2D>& samples,
    std::size_t maximumSurfaceCells) {
    LiquidSurfaceSnapshot2D snapshot;
    if (samples.empty()) return snapshot;
    maximumSurfaceCells = std::clamp<std::size_t>(
        maximumSurfaceCells, 1024, 200000);

    std::vector<LiquidSurfaceSample2D> validSamples;
    constexpr std::size_t maximumDensitySamples = 4096;
    validSamples.reserve(std::min(samples.size(), maximumDensitySamples));
    double sizeSum = 0.0;
    const std::size_t sampleStride = std::max<std::size_t>(
        1, (samples.size() + maximumDensitySamples - 1) /
               maximumDensitySamples);
    constexpr float maximumSurfaceCoordinate = 10000000.0f;
    constexpr float maximumSurfaceSampleSize = 1000000.0f;
    for (std::size_t sampleIndex = 0; sampleIndex < samples.size();
         sampleIndex += sampleStride) {
        const auto& sample = samples[sampleIndex];
        if (!std::isfinite(sample.x) || !std::isfinite(sample.y) ||
            !std::isfinite(sample.size) || sample.size <= 0.0f ||
            !std::isfinite(sample.vx) || !std::isfinite(sample.vy) ||
            !std::isfinite(sample.foamBias) ||
            !std::isfinite(sample.collisionImpact) ||
            std::abs(sample.x) > maximumSurfaceCoordinate ||
            std::abs(sample.y) > maximumSurfaceCoordinate ||
            sample.size > maximumSurfaceSampleSize) {
            continue;
        }
        validSamples.push_back(sample);
        sizeSum += sample.size;
        if (validSamples.size() >= maximumDensitySamples) break;
    }
    if (validSamples.empty()) return snapshot;

    const float averageSize = static_cast<float>(
        sizeSum / static_cast<double>(validSamples.size()));
    const float lodScale = std::min(
        5.0f, std::sqrt(static_cast<float>(sampleStride)));
    const float gridSpacing =
        std::max(2.0f, averageSize * 0.58f * lodScale);
    std::map<std::pair<int, int>, float> densityNodes;
    for (const auto& sample : validSamples) {
        const float radius = std::min(
            std::max(1.0f, sample.size * 1.15f * lodScale),
            gridSpacing * 8.0f);
        const int minimumNodeX = static_cast<int>(
            std::floor((sample.x - radius) / gridSpacing));
        const int maximumNodeX = static_cast<int>(
            std::ceil((sample.x + radius) / gridSpacing));
        const int minimumNodeY = static_cast<int>(
            std::floor((sample.y - radius) / gridSpacing));
        const int maximumNodeY = static_cast<int>(
            std::ceil((sample.y + radius) / gridSpacing));
        for (int nodeY = minimumNodeY; nodeY <= maximumNodeY; ++nodeY) {
            const float worldY = static_cast<float>(nodeY) * gridSpacing;
            for (int nodeX = minimumNodeX; nodeX <= maximumNodeX; ++nodeX) {
                const float worldX = static_cast<float>(nodeX) * gridSpacing;
                const float dx = worldX - sample.x;
                const float dy = worldY - sample.y;
                const float normalizedDistanceSquared =
                    (dx * dx + dy * dy) / (radius * radius);
                if (normalizedDistanceSquared >= 1.0f) continue;
                const float influence = 1.0f - normalizedDistanceSquared;
                densityNodes[{nodeX, nodeY}] += influence * influence;
            }
        }
    }

    constexpr std::size_t maximumFoamPoints = 4096;
    snapshot.foamPoints.reserve(
        std::min(validSamples.size(), maximumFoamPoints));
    for (const auto& sample : validSamples) {
        if (snapshot.foamPoints.size() >= maximumFoamPoints) break;
        const float speed = std::hypot(sample.vx, sample.vy);
        const float speedThreshold = std::max(12.0f, sample.size * 1.8f);
        if (!std::isfinite(speed)) continue;
        const float impactThreshold = std::max(8.0f, sample.size * 1.2f);
        const float impact = std::clamp(
            sample.collisionImpact / (impactThreshold * 3.0f),
            0.0f, 1.0f);
        if (speed <= speedThreshold && impact <= 0.08f) continue;
        const int nodeX = static_cast<int>(
            std::lround(sample.x / gridSpacing));
        const int nodeY = static_cast<int>(
            std::lround(sample.y / gridSpacing));
        const auto densityNode = densityNodes.find({nodeX, nodeY});
        const float localDensity = densityNode != densityNodes.end()
            ? densityNode->second
            : 0.0f;
        const float exposure = std::clamp(
            (1.75f - localDensity) / 1.25f, 0.0f, 1.0f);
        const float motion = std::clamp(
            (speed - speedThreshold) / (speedThreshold * 2.5f),
            0.0f, 1.0f);
        const float effectiveExposure =
            std::max(exposure, impact * 0.48f);
        const float activity = std::max(motion, impact);
        const float intensity = activity * effectiveExposure *
            std::clamp(sample.foamBias, 0.0f, 1.0f);
        if (intensity <= 0.08f) continue;
        snapshot.foamPoints.push_back(
            {{sample.x, sample.y},
             std::max(1.0f, sample.size * (0.18f + intensity * 0.22f)),
             0.18f + intensity * 0.62f});
    }

    std::set<std::pair<int, int>> surfaceCells;
    for (const auto& [node, value] : densityNodes) {
        if (value <= 0.0f) continue;
        for (int offsetY = -1; offsetY <= 0; ++offsetY) {
            for (int offsetX = -1; offsetX <= 0; ++offsetX) {
                if (surfaceCells.size() >= maximumSurfaceCells) break;
                surfaceCells.insert(
                    {node.first + offsetX, node.second + offsetY});
            }
        }
        if (surfaceCells.size() >= maximumSurfaceCells) break;
    }

    struct SurfaceVertex {
        LiquidContainerPoint2D point;
        float density = 0.0f;
    };
    constexpr float threshold = 0.34f;
    constexpr std::size_t maximumTriangles = 100000;
    constexpr std::size_t maximumContourSegments = 20000;
    const auto interpolate = [&](const SurfaceVertex& from,
                                 const SurfaceVertex& to) {
        const float denominator = to.density - from.density;
        const float t = std::abs(denominator) > 1.0e-8f
            ? std::clamp((threshold - from.density) / denominator, 0.0f, 1.0f)
            : 0.5f;
        return SurfaceVertex{{from.point.x + (to.point.x - from.point.x) * t,
                              from.point.y + (to.point.y - from.point.y) * t},
                             threshold};
    };
    const auto emitClippedTriangle = [&](const SurfaceVertex& a,
                                         const SurfaceVertex& b,
                                         const SurfaceVertex& c) {
        std::vector<SurfaceVertex> input{a, b, c};
        std::vector<SurfaceVertex> output;
        std::vector<LiquidContainerPoint2D> intersections;
        output.reserve(4);
        intersections.reserve(2);
        SurfaceVertex previous = input.back();
        bool previousInside = previous.density >= threshold;
        for (const auto& current : input) {
            const bool currentInside = current.density >= threshold;
            if (currentInside != previousInside) {
                const SurfaceVertex intersection =
                    interpolate(previous, current);
                output.push_back(intersection);
                intersections.push_back(intersection.point);
            }
            if (currentInside) output.push_back(current);
            previous = current;
            previousInside = currentInside;
        }
        if (intersections.size() == 2 &&
            snapshot.contourSegments.size() < maximumContourSegments) {
            const float dx = intersections[1].x - intersections[0].x;
            const float dy = intersections[1].y - intersections[0].y;
            if (dx * dx + dy * dy > 1.0e-8f) {
                snapshot.contourSegments.push_back(
                    {intersections[0], intersections[1]});
            }
        }
        if (output.size() < 3) return;
        for (std::size_t i = 1; i + 1 < output.size(); ++i) {
            if (snapshot.triangles.size() >= maximumTriangles) return;
            const float averageDensity =
                (output[0].density + output[i].density +
                 output[i + 1].density) / 3.0f;
            const float thickness = std::clamp(
                (averageDensity - threshold) / 1.5f, 0.0f, 1.0f);
            snapshot.triangles.push_back(
                {output[0].point, output[i].point, output[i + 1].point,
                 thickness});
        }
    };

    snapshot.triangles.reserve(std::min<std::size_t>(
        maximumTriangles, surfaceCells.size() * 4));
    snapshot.contourSegments.reserve(std::min<std::size_t>(
        maximumContourSegments, surfaceCells.size() * 2));
    const auto densityAt = [&densityNodes](int x, int y) {
        const auto found = densityNodes.find({x, y});
        return found != densityNodes.end() ? found->second : 0.0f;
    };
    for (const auto& cell : surfaceCells) {
            const int x = cell.first;
            const int y = cell.second;
            const float left = static_cast<float>(x) * gridSpacing;
            const float top = static_cast<float>(y) * gridSpacing;
            const SurfaceVertex corners[4] = {
                {{left, top}, densityAt(x, y)},
                {{left + gridSpacing, top}, densityAt(x + 1, y)},
                {{left + gridSpacing, top + gridSpacing},
                 densityAt(x + 1, y + 1)},
                {{left, top + gridSpacing}, densityAt(x, y + 1)}};
            const SurfaceVertex center{
                {left + gridSpacing * 0.5f, top + gridSpacing * 0.5f},
                (corners[0].density + corners[1].density +
                 corners[2].density + corners[3].density) * 0.25f};
            emitClippedTriangle(corners[0], corners[1], center);
            emitClippedTriangle(corners[1], corners[2], center);
            emitClippedTriangle(corners[2], corners[3], center);
            emitClippedTriangle(corners[3], corners[0], center);
            if (snapshot.triangles.size() >= maximumTriangles) return snapshot;
    }
    return snapshot;
}

void LiquidSolver2D::reset(float fillAmount, float particleSpacing) {
    fillAmount_ = std::clamp(fillAmount, 0.0f, 1.0f);
    particleSpacing_ = std::clamp(particleSpacing, 0.025f, 0.2f);
    particles_.clear();
    if (fillAmount_ <= 0.0f) return;

    const float radius = particleSpacing_ * 0.5f;
    const float top = std::clamp(1.0f - fillAmount_, radius, 1.0f - radius);
    const int columns = std::max(1, static_cast<int>((1.0f - particleSpacing_) /
                                                      particleSpacing_));
    const int rows = std::max(1, static_cast<int>((1.0f - radius - top) /
                                                   particleSpacing_) + 1);
    particles_.reserve(static_cast<std::size_t>(columns * rows));
    for (int row = 0; row < rows; ++row) {
        const float y = 1.0f - radius - static_cast<float>(row) * particleSpacing_;
        if (y < top) break;
        const float stagger = (row & 1) ? radius : 0.0f;
        for (int column = 0; column < columns; ++column) {
            const float x = radius + stagger +
                            static_cast<float>(column) * particleSpacing_;
            if (x > 1.0f - radius) break;
            particles_.push_back({x, y, 0.0f, 0.0f});
        }
    }
    if (!containerPolygon_.empty()) {
        particles_.erase(
            std::remove_if(particles_.begin(), particles_.end(),
                [this](const LiquidParticle2D& particle) {
                    return !liquidPointInsidePolygon(
                        {particle.x, particle.y}, containerPolygon_);
                }),
            particles_.end());
        for (auto& particle : particles_) solveContainerBounds(particle);
    }
}

void LiquidSolver2D::setGravity(float x, float y) {
    if (std::isfinite(x) && std::isfinite(y)) {
        gravityX_ = x;
        gravityY_ = y;
    }
}

void LiquidSolver2D::setViscosity(float value) {
    viscosity_ = std::clamp(value, 0.0f, 1.0f);
}

void LiquidSolver2D::setSurfaceTension(float value) {
    surfaceTension_ = std::clamp(value, 0.0f, 1.0f);
}

void LiquidSolver2D::setSubsteps(int value) {
    substeps_ = std::clamp(value, 1, 8);
}

void LiquidSolver2D::setSolverIterations(int value) {
    solverIterations_ = std::clamp(value, 1, 12);
}

void LiquidSolver2D::solveContainerBounds(LiquidParticle2D& particle) const {
    const float radius = particleSpacing_ * 0.5f;
    const float damping = 0.28f;

    if (!containerPolygon_.empty()) {
        const LiquidContainerPoint2D point{particle.x, particle.y};
        const bool inside = liquidPointInsidePolygon(point, containerPolygon_);
        float closestDistanceSquared = std::numeric_limits<float>::max();
        std::size_t closestEdge = 0;
        LiquidContainerPoint2D closestPoint;
        for (std::size_t i = 0; i < containerPolygon_.size(); ++i) {
            const auto candidate = liquidClosestPointOnSegment(
                point, containerPolygon_[i],
                containerPolygon_[(i + 1) % containerPolygon_.size()]);
            const float dx = point.x - candidate.x;
            const float dy = point.y - candidate.y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < closestDistanceSquared) {
                closestDistanceSquared = distanceSquared;
                closestEdge = i;
                closestPoint = candidate;
            }
        }
        if (closestEdge == openContainerEdge_) return;
        const float distance = std::sqrt(std::max(closestDistanceSquared,
                                                   1.0e-12f));
        if (inside && distance >= radius) return;

        const auto& a = containerPolygon_[closestEdge];
        const auto& b = containerPolygon_[
            (closestEdge + 1) % containerPolygon_.size()];
        float normalX = -(b.y - a.y);
        float normalY = b.x - a.x;
        const float normalLength = std::hypot(normalX, normalY);
        if (normalLength <= 1.0e-8f) return;
        normalX /= normalLength;
        normalY /= normalLength;
        const LiquidContainerPoint2D probe{
            closestPoint.x + normalX * radius,
            closestPoint.y + normalY * radius};
        if (!liquidPointInsidePolygon(probe, containerPolygon_)) {
            normalX = -normalX;
            normalY = -normalY;
        }
        particle.x = closestPoint.x + normalX * radius;
        particle.y = closestPoint.y + normalY * radius;
        const float normalVelocity =
            particle.vx * normalX + particle.vy * normalY;
        if (normalVelocity < 0.0f) {
            particle.collisionImpact = std::max(
                particle.collisionImpact, -normalVelocity);
            particle.vx -= (1.0f + damping) * normalVelocity * normalX;
            particle.vy -= (1.0f + damping) * normalVelocity * normalY;
        }
        return;
    }

    // Side walls end at the open top. Once a particle has crossed the lip it
    // can spill sideways instead of being trapped by an infinite wall.
    if (particle.y >= 0.0f) {
        if (particle.x < radius) {
            particle.x = radius;
            if (particle.vx < 0.0f) {
                particle.collisionImpact = std::max(
                    particle.collisionImpact, -particle.vx);
                particle.vx *= -damping;
            }
        } else if (particle.x > 1.0f - radius) {
            particle.x = 1.0f - radius;
            if (particle.vx > 0.0f) {
                particle.collisionImpact = std::max(
                    particle.collisionImpact, particle.vx);
                particle.vx *= -damping;
            }
        }
    }
    if (particle.y > 1.0f - radius) {
        particle.y = 1.0f - radius;
        if (particle.vy > 0.0f) {
            particle.collisionImpact = std::max(
                particle.collisionImpact, particle.vy);
            particle.vy *= -damping;
        }
    }
}

bool LiquidSolver2D::escapedThroughContainerOpening(
    const LiquidParticle2D& particle) const {
    if (containerPolygon_.empty()) {
        return particle.y < -particleSpacing_ * 0.5f;
    }
    const LiquidContainerPoint2D point{particle.x, particle.y};
    if (liquidPointInsidePolygon(point, containerPolygon_)) return false;
    float closestDistanceSquared = std::numeric_limits<float>::max();
    std::size_t closestEdge = 0;
    for (std::size_t i = 0; i < containerPolygon_.size(); ++i) {
        const auto closest = liquidClosestPointOnSegment(
            point, containerPolygon_[i],
            containerPolygon_[(i + 1) % containerPolygon_.size()]);
        const float dx = point.x - closest.x;
        const float dy = point.y - closest.y;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared < closestDistanceSquared) {
            closestDistanceSquared = distanceSquared;
            closestEdge = i;
        }
    }
    return closestEdge == openContainerEdge_ &&
           closestDistanceSquared > particleSpacing_ * particleSpacing_ * 0.25f;
}

void LiquidSolver2D::solveParticleDistances() {
    const float restDistance = particleSpacing_ * 0.92f;
    const float restDistanceSquared = restDistance * restDistance;
    const float supportDistance = surfaceTension_ > 0.0f
        ? particleSpacing_ * 1.55f
        : restDistance;
    const float supportDistanceSquared = supportDistance * supportDistance;
    const float stiffness = 0.42f + surfaceTension_ * 0.28f;
    forEachLiquidNeighborPair(
        particles_, supportDistance,
        [this, restDistance, restDistanceSquared, supportDistance,
         supportDistanceSquared, stiffness](
            std::size_t i, std::size_t j) {
            float dx = particles_[j].x - particles_[i].x;
            float dy = particles_[j].y - particles_[i].y;
            float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared >= supportDistanceSquared) return;
            if (distanceSquared < 1.0e-10f) {
                dx = (i & 1U) ? 1.0e-4f : -1.0e-4f;
                dy = 0.0f;
                distanceSquared = dx * dx;
            }
            const float distance = std::sqrt(std::max(distanceSquared, 1.0e-10f));
            float correctionDistance = 0.0f;
            if (distanceSquared < restDistanceSquared) {
                correctionDistance =
                    (restDistance - distance) * stiffness;
            } else if (surfaceTension_ > 0.0f) {
                const float supportRange =
                    std::max(supportDistance - restDistance, 1.0e-6f);
                const float falloff = std::clamp(
                    1.0f - (distance - restDistance) / supportRange,
                    0.0f, 1.0f);
                correctionDistance =
                    -(distance - restDistance) * surfaceTension_ *
                    0.06f * falloff;
            }
            const float correction =
                correctionDistance * 0.5f / distance;
            const float cx = dx * correction;
            const float cy = dy * correction;
            particles_[i].x -= cx;
            particles_[i].y -= cy;
            particles_[j].x += cx;
            particles_[j].y += cy;
        });
}

void LiquidSolver2D::applyViscosity() {
    if (viscosity_ <= 0.0f) return;
    const float influenceDistance = particleSpacing_ * 1.65f;
    const float influenceDistanceSquared = influenceDistance * influenceDistance;
    const float blend = viscosity_ * 0.08f;
    forEachLiquidNeighborPair(
        particles_, influenceDistance,
        [this, influenceDistanceSquared, blend](
            std::size_t i, std::size_t j) {
            const float dx = particles_[j].x - particles_[i].x;
            const float dy = particles_[j].y - particles_[i].y;
            if (dx * dx + dy * dy > influenceDistanceSquared) return;
            const float averageVx = (particles_[i].vx + particles_[j].vx) * 0.5f;
            const float averageVy = (particles_[i].vy + particles_[j].vy) * 0.5f;
            particles_[i].vx += (averageVx - particles_[i].vx) * blend;
            particles_[i].vy += (averageVy - particles_[i].vy) * blend;
            particles_[j].vx += (averageVx - particles_[j].vx) * blend;
            particles_[j].vy += (averageVy - particles_[j].vy) * blend;
        });
}

void LiquidSolver2D::update(float dt) {
    if (dt <= 0.0f || particles_.empty()) return;
    dt = std::min(dt, 0.05f);
    const float stepDt = dt / static_cast<float>(substeps_);
    std::vector<std::pair<float, float>> previous(particles_.size());
    for (int substep = 0; substep < substeps_; ++substep) {
        const float impactRetention = std::exp(-6.0f * stepDt);
        for (std::size_t i = 0; i < particles_.size(); ++i) {
            auto& particle = particles_[i];
            particle.collisionImpact *= impactRetention;
            if (!std::isfinite(particle.collisionImpact) ||
                particle.collisionImpact < 0.0f) {
                particle.collisionImpact = 0.0f;
            }
            previous[i] = {particle.x, particle.y};
            particle.vx += gravityX_ * stepDt;
            particle.vy += gravityY_ * stepDt;
            particle.x += particle.vx * stepDt;
            particle.y += particle.vy * stepDt;
            solveContainerBounds(particle);
        }
        for (int iteration = 0; iteration < solverIterations_; ++iteration) {
            solveParticleDistances();
            for (auto& particle : particles_) solveContainerBounds(particle);
        }
        for (std::size_t i = 0; i < particles_.size(); ++i) {
            particles_[i].vx = (particles_[i].x - previous[i].first) / stepDt;
            particles_[i].vy = (particles_[i].y - previous[i].second) / stepDt;
        }
        applyViscosity();
    }

    for (auto& particle : particles_) {
        if (!std::isfinite(particle.x) || !std::isfinite(particle.y) ||
            !std::isfinite(particle.vx) || !std::isfinite(particle.vy) ||
            !std::isfinite(particle.collisionImpact)) {
            particle = {0.5f, 0.5f, 0.0f, 0.0f};
        }
    }
}

LiquidSnapshot2D LiquidSolver2D::snapshot() const {
    return {particles_};
}

bool LiquidSolver2D::restore(const LiquidSnapshot2D& snapshot) {
    constexpr std::size_t maxCheckpointParticles = 100000;
    if (snapshot.particles.size() > maxCheckpointParticles) return false;

    std::vector<LiquidParticle2D> restored;
    restored.reserve(snapshot.particles.size());
    for (const auto& particle : snapshot.particles) {
        if (!std::isfinite(particle.x) || !std::isfinite(particle.y) ||
            !std::isfinite(particle.vx) || !std::isfinite(particle.vy) ||
            !std::isfinite(particle.collisionImpact) ||
            particle.collisionImpact < 0.0f) {
            return false;
        }
        restored.push_back(particle);
    }
    particles_ = std::move(restored);
    return true;
}

std::vector<LiquidParticle2D> LiquidSolver2D::takeEscapedParticles() {
    std::vector<LiquidParticle2D> escaped;
    std::vector<LiquidParticle2D> retained;
    escaped.reserve(particles_.size() / 8);
    retained.reserve(particles_.size());
    for (const auto& particle : particles_) {
        if (escapedThroughContainerOpening(particle)) {
            escaped.push_back(particle);
        } else {
            retained.push_back(particle);
        }
    }
    particles_ = std::move(retained);
    return escaped;
}

} // namespace ArtifactCore
