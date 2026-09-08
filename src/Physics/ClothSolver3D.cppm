module;
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

export module Physics.Cloth3D;

import Container.NamedVector;
import Core.Parallel;

namespace ArtifactCore {

/**
 * @brief 3Dクロス基盤の質点 (2D SoftBodyとは別系)
 * PBD/XPBD参照のクリーン再実装。初段は伸縮+せん断のみ。
 */
export struct ClothPoint3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float prevX = 0.0f, prevY = 0.0f, prevZ = 0.0f;
    float mass = 1.0f;
    bool isPinned = false;
    float forceX = 0.0f, forceY = 0.0f, forceZ = 0.0f;
};

export struct ClothConstraint3D {
    int p1Idx = -1;
    int p2Idx = -1;
    float restDistance = 0.0f;
    float stiffness = 1.0f;
};

export struct ClothWind3D {
    float directionX = 0.0f;
    float directionY = 0.0f;
    float directionZ = 0.0f;
    float strength = 0.0f;
    float turbulence = 0.0f;
    float turbulenceFrequency = 1.0f;
};

export struct ClothSnapshot3D {
    std::vector<ClothPoint3D> points;
    std::vector<ClothConstraint3D> constraints;
    ClothWind3D wind;
    float turbulenceTime = 0.0f;
    float accumulatedTime = 0.0f;
    int gridColumns = 0;
    int gridRows = 0;
};

export struct ClothCollider3D {
    enum class Type {
        Plane,
        Sphere
    };

    Type type = Type::Plane;
    // Plane: normal + distance (n . p = distance)
    float normalX = 0.0f, normalY = 1.0f, normalZ = 0.0f;
    float planeDistance = 0.0f;
    // Sphere: center + radius
    float centerX = 0.0f, centerY = 0.0f, centerZ = 0.0f;
    float radius = 50.0f;
    float friction = 0.15f;
    bool enabled = true;
};

/**
 * @brief Verlet積分の最小3Dクロスソルバー
 * SoftBody2Dと状態・スナップショットを共有しない。bending/self-collision/GPUは次段。
 */
export class ClothSolver3D {
public:
    ClothSolver3D() = default;

    void setGravity(float gx, float gy, float gz) {
        gravityX_ = gx;
        gravityY_ = gy;
        gravityZ_ = gz;
    }

    void setConstraintIterations(int iterations) {
        constraintIterations_ = std::max(1, iterations);
    }

    void setCollisionIterations(int iterations) {
        collisionIterations_ = std::max(1, iterations);
    }

    void setFixedTimeStep(float seconds) {
        fixedTimeStep_ = std::clamp(seconds, 1.0f / 1000.0f, 1.0f / 15.0f);
    }

    float fixedTimeStep() const { return fixedTimeStep_; }

    void setMaxSubsteps(int count) {
        maxSubsteps_ = std::clamp(count, 1, 32);
    }

    int maxSubsteps() const { return maxSubsteps_; }

    void setWind(float dirX, float dirY, float dirZ, float strength) {
        wind_.directionX = dirX;
        wind_.directionY = dirY;
        wind_.directionZ = dirZ;
        wind_.strength = strength;
    }

    void setTurbulence(float strength, float frequency = 1.0f) {
        wind_.turbulence = strength;
        wind_.turbulenceFrequency = frequency;
    }

    const ClothWind3D& getWind() const { return wind_; }
    void clearWind() { wind_ = {}; }

    ClothSnapshot3D snapshot() const {
        ClothSnapshot3D out;
        out.points = points_.toStdVector();
        out.constraints = constraints_.toStdVector();
        out.wind = wind_;
        out.turbulenceTime = turbulenceTime_;
        out.accumulatedTime = accumulatedTime_;
        out.gridColumns = gridColumns_;
        out.gridRows = gridRows_;
        return out;
    }

    bool canRestoreSnapshot(const ClothSnapshot3D& snapshot) const {
        const auto hasValidPoint = [&snapshot](int index) {
            return index >= 0 &&
                index < static_cast<int>(snapshot.points.size());
        };
        for (const auto& c : snapshot.constraints) {
            if (!hasValidPoint(c.p1Idx) || !hasValidPoint(c.p2Idx)) {
                return false;
            }
        }
        if (snapshot.gridColumns < 0 || snapshot.gridRows < 0 ||
            (snapshot.gridColumns > 0 && snapshot.gridRows > 0 &&
             snapshot.points.size() != static_cast<std::size_t>(
                 snapshot.gridColumns * snapshot.gridRows))) {
            return false;
        }
        return true;
    }

    bool restoreSnapshot(const ClothSnapshot3D& snapshot) {
        if (!canRestoreSnapshot(snapshot)) {
            return false;
        }
        points_.clear();
        for (const auto& p : snapshot.points) {
            points_.push_back(p);
        }
        constraints_.clear();
        for (const auto& c : snapshot.constraints) {
            constraints_.push_back(c);
        }
        wind_ = snapshot.wind;
        turbulenceTime_ = snapshot.turbulenceTime;
        accumulatedTime_ = std::clamp(snapshot.accumulatedTime, 0.0f,
                                      fixedTimeStep_ * static_cast<float>(maxSubsteps_));
        gridColumns_ = snapshot.gridColumns;
        gridRows_ = snapshot.gridRows;
        return true;
    }

    void addPoint(float x, float y, float z, float mass = 1.0f, bool pinned = false) {
        ClothPoint3D p;
        p.x = x; p.y = y; p.z = z;
        p.prevX = x; p.prevY = y; p.prevZ = z;
        p.mass = mass;
        p.isPinned = pinned;
        points_.push_back(p);
    }

    void addConstraint(int p1, int p2, float stiffness = 1.0f) {
        if (p1 < 0 || p2 < 0) return;
        if (p1 >= static_cast<int>(points_.size())) return;
        if (p2 >= static_cast<int>(points_.size())) return;
        const auto& pt1 = points_[static_cast<std::size_t>(p1)];
        const auto& pt2 = points_[static_cast<std::size_t>(p2)];
        const float dx = pt1.x - pt2.x;
        const float dy = pt1.y - pt2.y;
        const float dz = pt1.z - pt2.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        ClothConstraint3D c;
        c.p1Idx = p1;
        c.p2Idx = p2;
        c.restDistance = dist;
        c.stiffness = stiffness;
        constraints_.push_back(c);
    }

    /**
     * @brief XY平面の布グリッドを生成する (z=depth、上面行pin可)
     * @param shearStiffness 0以下で斜め拘束を省く
     */
    void buildGrid(float left,
                   float top,
                   float width,
                   float height,
                   float depth = 0.0f,
                   int columns = 8,
                   int rows = 8,
                   float pointMass = 1.0f,
                   float stiffness = 1.0f,
                   bool pinTopRow = true,
                   float shearStiffness = 0.5f) {
        clear();
        const int safeColumns = std::max(2, columns);
        const int safeRows = std::max(2, rows);
        gridColumns_ = safeColumns;
        gridRows_ = safeRows;
        const float stepX = width / static_cast<float>(safeColumns - 1);
        const float stepY = height / static_cast<float>(safeRows - 1);
        points_.reserve(static_cast<std::size_t>(safeColumns * safeRows));

        for (int y = 0; y < safeRows; ++y) {
            for (int x = 0; x < safeColumns; ++x) {
                const bool pinned = pinTopRow && y == 0;
                addPoint(left + stepX * static_cast<float>(x),
                         top + stepY * static_cast<float>(y),
                         depth, pointMass, pinned);
            }
        }

        const int baseIndex = 0;
        for (int y = 0; y < safeRows; ++y) {
            for (int x = 0; x < safeColumns; ++x) {
                const int idx = baseIndex + y * safeColumns + x;
                if (x + 1 < safeColumns) {
                    addConstraint(idx, idx + 1, stiffness);
                }
                if (y + 1 < safeRows) {
                    addConstraint(idx, idx + safeColumns, stiffness);
                }
                if (shearStiffness > 0.0f && x + 1 < safeColumns && y + 1 < safeRows) {
                    addConstraint(idx, idx + safeColumns + 1, shearStiffness);
                    addConstraint(idx + 1, idx + safeColumns, shearStiffness);
                }
            }
        }
    }

    int addCollider(const ClothCollider3D& collider) {
        colliders_.push_back(collider);
        return static_cast<int>(colliders_.size()) - 1;
    }

    void clearColliders() {
        colliders_.clear();
    }

    void update(float elapsedSeconds, float gravityX, float gravityY, float gravityZ) {
        if (points_.isEmpty() || elapsedSeconds <= 0.0f) return;
        const float maximumAccumulatedTime =
            fixedTimeStep_ * static_cast<float>(maxSubsteps_);
        accumulatedTime_ = std::min(
            accumulatedTime_ + std::min(elapsedSeconds, maximumAccumulatedTime),
            maximumAccumulatedTime);
        int substeps = 0;
        while (accumulatedTime_ + 1e-6f >= fixedTimeStep_ &&
               substeps < maxSubsteps_) {
            simulateStep(fixedTimeStep_, gravityX, gravityY, gravityZ);
            accumulatedTime_ -= fixedTimeStep_;
            ++substeps;
        }
    }

    void simulateStep(float dt, float gravityX, float gravityY, float gravityZ) {
        if (points_.isEmpty()) return;
        if (dt <= 0.0f) return;

        gravityX_ = gravityX;
        gravityY_ = gravityY;
        gravityZ_ = gravityZ;
        dt = std::min(dt, 0.05f);

        turbulenceTime_ += dt * wind_.turbulenceFrequency;

        Parallel::For(0, static_cast<int>(points_.size()),
                      static_cast<int>(points_.size()), [&](int pointIndex) {
            auto& p = points_[static_cast<std::size_t>(pointIndex)];
            if (p.isPinned) return;

            float vx = p.x - p.prevX;
            float vy = p.y - p.prevY;
            float vz = p.z - p.prevZ;

            const float windForceX = wind_.directionX * wind_.strength;
            const float windForceY = wind_.directionY * wind_.strength;
            const float windForceZ = wind_.directionZ * wind_.strength;
            const float turbX =
                std::sin(turbulenceTime_ + p.x * 0.01f) * wind_.turbulence;
            const float turbY =
                std::cos(turbulenceTime_ + p.y * 0.01f) * wind_.turbulence;
            const float turbZ =
                std::sin(turbulenceTime_ * 0.9f + p.z * 0.01f) * wind_.turbulence;

            vx += (gravityX + windForceX + turbX + p.forceX) * dt * dt;
            vy += (gravityY + windForceY + turbY + p.forceY) * dt * dt;
            vz += (gravityZ + windForceZ + turbZ + p.forceZ) * dt * dt;
            p.forceX = 0.0f; p.forceY = 0.0f; p.forceZ = 0.0f;

            vx *= 0.999f;
            vy *= 0.999f;
            vz *= 0.999f;

            p.prevX = p.x;
            p.prevY = p.y;
            p.prevZ = p.z;
            p.x += vx;
            p.y += vy;
            p.z += vz;
        });

        for (int iter = 0; iter < constraintIterations_; ++iter) {
            for (std::size_t ci = 0; ci < constraints_.size(); ++ci) {
                auto& c = constraints_[ci];
                auto& p1 = points_[static_cast<std::size_t>(c.p1Idx)];
                auto& p2 = points_[static_cast<std::size_t>(c.p2Idx)];
                const float dx = p2.x - p1.x;
                const float dy = p2.y - p1.y;
                const float dz = p2.z - p1.z;
                const float currentDist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (currentDist < 1e-6f) continue;
                const float delta = (currentDist - c.restDistance) / currentDist;
                const float forceX = dx * 0.5f * delta * c.stiffness;
                const float forceY = dy * 0.5f * delta * c.stiffness;
                const float forceZ = dz * 0.5f * delta * c.stiffness;
                if (!p1.isPinned) { p1.x += forceX; p1.y += forceY; p1.z += forceZ; }
                if (!p2.isPinned) { p2.x -= forceX; p2.y -= forceY; p2.z -= forceZ; }
            }
        }

        for (int i = 0; i < collisionIterations_; ++i) {
            for (std::size_t pi = 0; pi < points_.size(); ++pi) {
                auto& p = points_[pi];
                if (p.isPinned) continue;
                resolveColliders(p);
            }
        }

        for (std::size_t pi = 0; pi < points_.size(); ++pi) {
            auto& p = points_[pi];
            const bool posBad = !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z);
            const bool prevBad =
                !std::isfinite(p.prevX) || !std::isfinite(p.prevY) || !std::isfinite(p.prevZ);
            if (!posBad && !prevBad) continue;
            if (posBad) {
                p.x = prevBad ? 0.0f : p.prevX;
                p.y = prevBad ? 0.0f : p.prevY;
                p.z = prevBad ? 0.0f : p.prevZ;
            }
            p.prevX = p.x;
            p.prevY = p.y;
            p.prevZ = p.z;
        }
    }

    void clear() {
        points_.clear();
        constraints_.clear();
        colliders_.clear();
        wind_ = {};
        gridColumns_ = 0;
        gridRows_ = 0;
        turbulenceTime_ = 0.0f;
        accumulatedTime_ = 0.0f;
    }

    std::size_t pointCount() const noexcept { return points_.size(); }
    std::size_t constraintCount() const noexcept { return constraints_.size(); }
    const ClothPoint3D& point(std::size_t i) const { return points_[i]; }
    int gridColumns() const { return gridColumns_; }
    int gridRows() const { return gridRows_; }

    std::vector<std::uint32_t> getGridTriangleIndices() const {
        std::vector<std::uint32_t> indices;
        if (gridColumns_ < 2 || gridRows_ < 2 ||
            points_.size() != static_cast<std::size_t>(gridColumns_ * gridRows_)) {
            return indices;
        }
        indices.reserve(static_cast<std::size_t>((gridColumns_ - 1) * (gridRows_ - 1) * 6));
        const auto index = [this](int x, int y) {
            return static_cast<std::uint32_t>(y * gridColumns_ + x);
        };
        for (int y = 0; y + 1 < gridRows_; ++y) {
            for (int x = 0; x + 1 < gridColumns_; ++x) {
                const std::uint32_t p0 = index(x, y);
                const std::uint32_t p1 = index(x + 1, y);
                const std::uint32_t p2 = index(x + 1, y + 1);
                const std::uint32_t p3 = index(x, y + 1);
                indices.push_back(p0);
                indices.push_back(p1);
                indices.push_back(p2);
                indices.push_back(p0);
                indices.push_back(p2);
                indices.push_back(p3);
            }
        }
        return indices;
    }

private:
    void resolveColliders(ClothPoint3D& p) {
        for (std::size_t ci = 0; ci < colliders_.size(); ++ci) {
            const auto& collider = colliders_[ci];
            if (!collider.enabled) continue;
            if (collider.type == ClothCollider3D::Type::Plane) {
                resolvePlane(p, collider);
            } else {
                resolveSphere(p, collider);
            }
        }
    }

    void resolvePlane(ClothPoint3D& p, const ClothCollider3D& collider) {
        const float dist =
            p.x * collider.normalX + p.y * collider.normalY + p.z * collider.normalZ -
            collider.planeDistance;
        if (dist >= 0.0f) return;
        p.x -= collider.normalX * dist;
        p.y -= collider.normalY * dist;
        p.z -= collider.normalZ * dist;
    }

    void resolveSphere(ClothPoint3D& p, const ClothCollider3D& collider) {
        const float dx = p.x - collider.centerX;
        const float dy = p.y - collider.centerY;
        const float dz = p.z - collider.centerZ;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist >= collider.radius || dist < 1e-6f) return;
        const float push = (collider.radius - dist) / dist;
        p.x += dx * push;
        p.y += dy * push;
        p.z += dz * push;
    }

    NamedVector<ClothPoint3D> points_;
    NamedVector<ClothConstraint3D> constraints_;
    NamedVector<ClothCollider3D> colliders_;
    ClothWind3D wind_;
    float gravityX_ = 0.0f;
    float gravityY_ = 9.8f;
    float gravityZ_ = 0.0f;
    float turbulenceTime_ = 0.0f;
    float accumulatedTime_ = 0.0f;
    float fixedTimeStep_ = 1.0f / 60.0f;
    int maxSubsteps_ = 8;
    int constraintIterations_ = 4;
    int collisionIterations_ = 2;
    int gridColumns_ = 0;
    int gridRows_ = 0;
};

} // namespace ArtifactCore
