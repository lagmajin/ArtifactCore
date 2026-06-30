module;
#include <utility>
#include <vector>
#include <cmath>
#include <algorithm>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

export module Physics.SoftBody;

import Utils.Id;

namespace ArtifactCore {

/**
 * @brief ソフトボディ（布やゲル）を構成する質点
 */
export struct SoftBodyPoint {
    float x, y;
    float prevX, prevY;
    float mass;
    bool isPinned = false;
    float forceX = 0.0f, forceY = 0.0f;
};

/**
 * @brief 質点間の距離拘束（バネ）
 */
export struct SoftBodyConstraint {
    int p1Idx;
    int p2Idx;
    float restDistance;
    float stiffness = 1.0f;
    float accumulatedStress = 0.0f;
};

export struct SoftBodyVolumeTriangle {
    int i0, i1, i2;
    float restArea;
};

export struct SoftBodyWind {
    float directionX = 1.0f;
    float directionY = 0.0f;
    float strength = 0.0f;
    float turbulence = 0.0f;
    float turbulenceFrequency = 1.0f;
};

/**
 * @brief ソフトボディの衝突対象
 */
export struct SoftBodyCollider {
    enum class Type {
        Plane,
        Box,
        Circle
    };

    Type type = Type::Plane;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    float restitution = 0.25f;
    float friction = 0.15f;
    bool enabled = true;
};

/**
 * @brief Verlet 積分を用いたシンプルなソフトボディソルバー
 */
export class SoftBodySolver {
public:
    SoftBodySolver() = default;

    void setGravity(float gx, float gy) {
        gravityX_ = gx;
        gravityY_ = gy;
    }

    void setConstraintIterations(int iterations) {
        constraintIterations_ = std::max(1, iterations);
    }

    void setCollisionIterations(int iterations) {
        collisionIterations_ = std::max(1, iterations);
    }

    void setCollisionDamping(float damping) {
        collisionDamping_ = std::clamp(damping, 0.0f, 1.0f);
    }

    void addPoint(float x, float y, float mass = 1.0f, bool pinned = false) {
        points_.push_back({x, y, x, y, mass, pinned});
    }

    void addConstraint(int p1, int p2, float stiffness = 1.0f) {
        if (p1 >= points_.size() || p2 >= points_.size()) return;
        const auto& pt1 = points_[p1];
        const auto& pt2 = points_[p2];
        float dx = pt1.x - pt2.x;
        float dy = pt1.y - pt2.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        constraints_.push_back({p1, p2, dist, stiffness});
    }

    /**
     * @brief 長方形グリッドのソフトボディ（布）を生成する
     * @param shearStiffness せん断（斜め）拘束の強さ。0以下でスキップ
     * @param bendStiffness 曲げ（1スキップ）拘束の強さ。0以下でスキップ
     */
    void buildGrid(float left,
                   float top,
                   float width,
                   float height,
                   int columns,
                   int rows,
                   float pointMass = 1.0f,
                   float stiffness = 1.0f,
                   bool pinTopRow = true,
                   float shearStiffness = 0.5f,
                   float bendStiffness = 0.3f) {
        clear();

        const int safeColumns = std::max(2, columns);
        const int safeRows = std::max(2, rows);
        const float stepX = width / static_cast<float>(safeColumns - 1);
        const float stepY = height / static_cast<float>(safeRows - 1);
        const int baseIndex = static_cast<int>(points_.size());

        for (int y = 0; y < safeRows; ++y) {
            for (int x = 0; x < safeColumns; ++x) {
                const bool pinned = pinTopRow && y == 0;
                addPoint(left + stepX * static_cast<float>(x),
                         top + stepY * static_cast<float>(y),
                         pointMass,
                         pinned);
            }
        }

        for (int y = 0; y < safeRows; ++y) {
            for (int x = 0; x < safeColumns; ++x) {
                const int idx = baseIndex + y * safeColumns + x;
                // Structural: horizontal
                if (x + 1 < safeColumns) {
                    addConstraint(idx, idx + 1, stiffness);
                }
                // Structural: vertical
                if (y + 1 < safeRows) {
                    addConstraint(idx, idx + safeColumns, stiffness);
                }
                // Shear: diagonal (x,y) -> (x+1,y+1)
                if (shearStiffness > 0.0f && x + 1 < safeColumns && y + 1 < safeRows) {
                    addConstraint(idx, idx + safeColumns + 1, shearStiffness);
                }
                // Shear: anti-diagonal (x+1,y) -> (x,y+1)
                if (shearStiffness > 0.0f && x + 1 < safeColumns && y + 1 < safeRows) {
                    addConstraint(idx + 1, idx + safeColumns, shearStiffness);
                }
                // Bend: horizontal skip-one
                if (bendStiffness > 0.0f && x + 2 < safeColumns) {
                    addConstraint(idx, idx + 2, bendStiffness);
                }
                // Bend: vertical skip-one
                if (bendStiffness > 0.0f && y + 2 < safeRows) {
                    addConstraint(idx, idx + safeColumns * 2, bendStiffness);
                }
            }
        }
    }

    /**
     * @brief 直線チェーンのソフトボディを生成する
     */
    void buildChain(float startX,
                    float startY,
                    float endX,
                    float endY,
                    int segments,
                    float pointMass = 1.0f,
                    float stiffness = 1.0f,
                    bool pinEnds = false) {
        clear();

        const int safeSegments = std::max(2, segments);
        const int baseIndex = static_cast<int>(points_.size());
        for (int i = 0; i < safeSegments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(safeSegments - 1);
            const bool pinned = pinEnds && (i == 0 || i == safeSegments - 1);
            addPoint(startX + (endX - startX) * t,
                     startY + (endY - startY) * t,
                     pointMass,
                     pinned);
        }
        for (int i = 0; i + 1 < safeSegments; ++i) {
            addConstraint(baseIndex + i, baseIndex + i + 1, stiffness);
        }
    }

    int addCollider(const SoftBodyCollider& collider) {
        colliders_.push_back(collider);
        return static_cast<int>(colliders_.size()) - 1;
    }

    void clearColliders() {
        colliders_.clear();
    }

    // ─── Self-collision ───
    void setSelfCollisionEnabled(bool enabled) { selfCollisionEnabled_ = enabled; }
    bool isSelfCollisionEnabled() const { return selfCollisionEnabled_; }
    void setSelfCollisionRadius(float radius) { selfCollisionRadius_ = std::max(0.0f, radius); }
    float selfCollisionRadius() const { return selfCollisionRadius_; }

    // ─── Dynamic remeshing ───
    void setRemeshingEnabled(bool enabled) { remeshingEnabled_ = enabled; }
    bool isRemeshingEnabled() const { return remeshingEnabled_; }
    void setSubdivideThreshold(float t) { subdivideThreshold_ = std::max(1.1f, t); }
    void setCollapseThreshold(float t) { collapseThreshold_ = std::clamp(t, 0.0f, 0.9f); }
    void setMinSegmentLength(float len) { minSegmentLength_ = std::max(1.0f, len); }
    float subdivideThreshold() const { return subdivideThreshold_; }
    float collapseThreshold() const { return collapseThreshold_; }
    float minSegmentLength() const { return minSegmentLength_; }

    // ─── Tearing ───
    void setTearingEnabled(bool enabled) { tearingEnabled_ = enabled; }
    bool isTearingEnabled() const { return tearingEnabled_; }
    void setMaxStrain(float strain) { maxStrain_ = std::max(0.1f, strain); }
    float maxStrain() const { return maxStrain_; }
    int getConstraintCount() const { return static_cast<int>(constraints_.size()); }

    // ─── Volume preservation ───
    void addVolumeTriangle(int i0, int i1, int i2) {
        if (i0 >= static_cast<int>(points_.size()) ||
            i1 >= static_cast<int>(points_.size()) ||
            i2 >= static_cast<int>(points_.size())) return;
        auto& p0 = points_[i0], p1 = points_[i1], p2 = points_[i2];
        float area = 0.5f * std::abs((p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
        volumeTriangles_.push_back({i0, i1, i2, area});
    }

    void clearVolumeTriangles() { volumeTriangles_.clear(); }
    void setPressureStiffness(float stiffness) { pressureStiffness_ = std::clamp(stiffness, 0.0f, 1.0f); }
    float pressureStiffness() const { return pressureStiffness_; }

    // ─── Wind / Turbulence ───
    void setWind(float dirX, float dirY, float strength) {
        wind_.directionX = dirX; wind_.directionY = dirY; wind_.strength = strength;
    }
    void setTurbulence(float strength, float frequency = 1.0f) {
        wind_.turbulence = strength; wind_.turbulenceFrequency = frequency;
    }
    const SoftBodyWind& getWind() const { return wind_; }
    void clearWind() { wind_ = {}; }

    /**
     * @brief シミュレーションを 1 ステップ進める
     */
    void update(float dt, float gravityX, float gravityY, int iterations = 5) {
        if (points_.empty()) return;
        if (dt <= 0.0f) return;

        gravityX_ = gravityX;
        gravityY_ = gravityY;
        constraintIterations_ = std::max(1, iterations);
        dt = std::min(dt, 0.05f); // clamp to avoid explosion

        // Wind phase accumulation
        turbulenceTime_ += dt * wind_.turbulenceFrequency;

        // 1. 積分（移動）+ 外力（風/乱流）
        for (auto& p : points_) {
            if (p.isPinned) continue;

            float vx = (p.x - p.prevX);
            float vy = (p.y - p.prevY);

            // Wind force
            float windForceX = wind_.directionX * wind_.strength;
            float windForceY = wind_.directionY * wind_.strength;

            // Turbulence force (sinusoidal noise)
            float turbX = std::sin(turbulenceTime_ + p.x * 0.01f) * wind_.turbulence;
            float turbY = std::cos(turbulenceTime_ + p.y * 0.01f) * wind_.turbulence;

            // Accumulated external forces
            vx += (gravityX + windForceX + turbX + p.forceX) * dt * dt;
            vy += (gravityY + windForceY + turbY + p.forceY) * dt * dt;
            p.forceX = 0.0f; p.forceY = 0.0f;

            // Velocity damping (drag)
            vx *= 0.999f;
            vy *= 0.999f;

            p.prevX = p.x;
            p.prevY = p.y;

            p.x += vx;
            p.y += vy;
        }

        // 2. 拘束解決（反復計算）+ 破断検出
        std::vector<int> constraintsToRemove;
        for (int iter = 0; iter < constraintIterations_; ++iter) {
            for (size_t ci = 0; ci < constraints_.size(); ++ci) {
                auto& c = constraints_[ci];
                auto& p1 = points_[c.p1Idx];
                auto& p2 = points_[c.p2Idx];

                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float currentDist = std::sqrt(dx * dx + dy * dy);
                if (currentDist < 1e-6f) continue;

                // Strain for tearing
                float strain = currentDist / c.restDistance;
                c.accumulatedStress = std::max(c.accumulatedStress, strain);

                if (tearingEnabled_ && strain > maxStrain_) {
                    constraintsToRemove.push_back(static_cast<int>(ci));
                    continue;
                }

                float delta = (currentDist - c.restDistance) / currentDist;
                float forceX = dx * 0.5f * delta * c.stiffness;
                float forceY = dy * 0.5f * delta * c.stiffness;

                if (!p1.isPinned) { p1.x += forceX; p1.y += forceY; }
                if (!p2.isPinned) { p2.x -= forceX; p2.y -= forceY; }
            }
        }

        // Remove broken constraints (reverse order to keep indices valid)
        if (!constraintsToRemove.empty()) {
            std::sort(constraintsToRemove.begin(), constraintsToRemove.end(), std::greater<>());
            for (int ci : constraintsToRemove) {
                if (ci < static_cast<int>(constraints_.size()))
                    constraints_.erase(constraints_.begin() + ci);
            }
        }

        // 2b. 自己衝突解決 (点-線分)
        if (selfCollisionEnabled_ && selfCollisionRadius_ > 0.0f) {
            float sr = selfCollisionRadius_;
            float srSq = sr * sr;
            for (size_t ci = 0; ci < constraints_.size(); ++ci) {
                auto& c = constraints_[ci];
                for (auto& p : points_) {
                    // Skip: point is part of this edge
                    int ptIdx = static_cast<int>(&p - points_.data());
                    if (ptIdx == c.p1Idx || ptIdx == c.p2Idx) continue;
                    if (p.isPinned) continue;

                    auto& p1 = points_[c.p1Idx];
                    auto& p2 = points_[c.p2Idx];
                    float ex = p2.x - p1.x;
                    float ey = p2.y - p1.y;
                    float edgeLenSq = ex * ex + ey * ey;
                    if (edgeLenSq < 1e-8f) continue;

                    float t = ((p.x - p1.x) * ex + (p.y - p1.y) * ey) / edgeLenSq;
                    t = std::clamp(t, 0.0f, 1.0f);
                    float cx = p1.x + t * ex;
                    float cy = p1.y + t * ey;
                    float dx = p.x - cx;
                    float dy = p.y - cy;
                    float distSq = dx * dx + dy * dy;
                    if (distSq < srSq && distSq > 1e-8f) {
                        float dist = std::sqrt(distSq);
                        float push = (sr - dist) * 0.5f;
                        float invDist = 1.0f / dist;
                        p.x += dx * invDist * push;
                        p.y += dy * invDist * push;
                        // Also push the edge slightly
                        if (!p1.isPinned) { p1.x -= dx * invDist * push * 0.3f; p1.y -= dy * invDist * push * 0.3f; }
                        if (!p2.isPinned) { p2.x -= dx * invDist * push * 0.3f; p2.y -= dy * invDist * push * 0.3f; }
                    }
                }
            }
        }

        // 2c. 動的リメッシュ（subdivision / collapse）
        if (remeshingEnabled_) {
            // Collection phase: gather splits and collapses
            std::vector<std::pair<int, int>> splits; // constraint index, new point index
            std::vector<int> collapses; // constraint index to collapse
            for (size_t ci = 0; ci < constraints_.size(); ++ci) {
                auto& c = constraints_[ci];
                auto& p1 = points_[c.p1Idx];
                auto& p2 = points_[c.p2Idx];
                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float currentDist = std::sqrt(dx * dx + dy * dy);
                if (currentDist < 1e-6f) continue;
                float strain = currentDist / c.restDistance;
                if (strain > subdivideThreshold_ && currentDist > minSegmentLength_) {
                    int newIdx = static_cast<int>(points_.size());
                    points_.push_back({
                        (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f,
                        (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f,
                        (p1.mass + p2.mass) * 0.5f, false
                    });
                    splits.emplace_back(static_cast<int>(ci), newIdx);
                } else if (strain < collapseThreshold_) {
                    collapses.push_back(static_cast<int>(ci));
                }
            }
            // Apply splits (reverse order to keep indices)
            for (auto it = splits.rbegin(); it != splits.rend(); ++it) {
                int ci = it->first;
                int newIdx = it->second;
                if (ci >= static_cast<int>(constraints_.size())) continue;
                auto& c = constraints_[ci];
                int oldP2 = c.p2Idx;
                float halfRest = c.restDistance * 0.5f;
                // Replace original constraint: p1 <-> new
                c.p2Idx = newIdx;
                c.restDistance = halfRest;
                // Add new constraint: new <-> oldP2
                addConstraint(newIdx, oldP2, c.stiffness);
            }
            // Apply collapses: merge p1 into p2, redirect all constraints referencing p1
            for (int ci : collapses) {
                if (ci >= static_cast<int>(constraints_.size())) continue;
                auto& c = constraints_[ci];
                int keepIdx = c.p2Idx;
                int removeIdx = c.p1Idx;
                if (points_[removeIdx].isPinned && !points_[keepIdx].isPinned)
                    std::swap(keepIdx, removeIdx);
                // Redirect all constraints referencing removeIdx to keepIdx
                for (auto& oc : constraints_) {
                    if (oc.p1Idx == removeIdx) oc.p1Idx = keepIdx;
                    if (oc.p2Idx == removeIdx) oc.p2Idx = keepIdx;
                }
                // Remove the constraint itself (will be removed below)
            }
            // Clean degenerate constraints (self-referencing)
            constraints_.erase(
                std::remove_if(constraints_.begin(), constraints_.end(),
                    [](const SoftBodyConstraint& x) { return x.p1Idx == x.p2Idx; }),
                constraints_.end());
        }

        // 3. 体積保存（圧力拘束）
        if (pressureStiffness_ > 0.0f && !volumeTriangles_.empty()) {
            for (auto& tri : volumeTriangles_) {
                auto& p0 = points_[tri.i0];
                auto& p1 = points_[tri.i1];
                auto& p2 = points_[tri.i2];
                float area = 0.5f * std::abs(
                    (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y));
                float pressure = (area - tri.restArea) / (tri.restArea + 1e-6f);
                float correction = pressure * pressureStiffness_;

                // Gradient: force at each vertex is perpendicular to opposite edge
                auto applyPressure = [&](SoftBodyPoint& pi, const SoftBodyPoint& pj, const SoftBodyPoint& pk) {
                    if (pi.isPinned) return;
                    float nx = -(pk.y - pj.y) * correction;
                    float ny = (pk.x - pj.x) * correction;
                    pi.x += nx;
                    pi.y += ny;
                };
                applyPressure(p0, p1, p2);
                applyPressure(p1, p2, p0);
                applyPressure(p2, p0, p1);
            }
        }

        // 4. 衝突解決
        for (int i = 0; i < collisionIterations_; ++i) {
            for (auto& p : points_) {
                if (p.isPinned) continue;
                resolveColliders(p);
            }
        }
    }

    void clear() {
        points_.clear();
        constraints_.clear();
        colliders_.clear();
        volumeTriangles_.clear();
        wind_ = {};
    }

    const std::vector<SoftBodyPoint>& getPoints() const { return points_; }
    const std::vector<SoftBodyConstraint>& getConstraints() const { return constraints_; }
    const std::vector<SoftBodyCollider>& getColliders() const { return colliders_; }
    const std::vector<SoftBodyVolumeTriangle>& getVolumeTriangles() const { return volumeTriangles_; }

private:
    void resolveColliders(SoftBodyPoint& p) {
        for (const auto& collider : colliders_) {
            if (!collider.enabled) {
                continue;
            }
            switch (collider.type) {
            case SoftBodyCollider::Type::Plane:
                resolvePlane(p, collider);
                break;
            case SoftBodyCollider::Type::Box:
                resolveBox(p, collider);
                break;
            case SoftBodyCollider::Type::Circle:
                resolveCircle(p, collider);
                break;
            }
        }
    }

    void resolvePlane(SoftBodyPoint& p, const SoftBodyCollider& collider) {
        const float planeY = collider.y;
        if (p.y >= planeY) {
            return;
        }
        p.y = planeY;
        p.prevY = p.y + (p.prevY - p.y) * collider.restitution;
        p.prevX = p.x + (p.prevX - p.x) * (1.0f - collider.friction * collisionDamping_);
    }

    void resolveBox(SoftBodyPoint& p, const SoftBodyCollider& collider) {
        const float halfW = std::max(0.0f, collider.width * 0.5f);
        const float halfH = std::max(0.0f, collider.height * 0.5f);
        const float minX = collider.x - halfW;
        const float maxX = collider.x + halfW;
        const float minY = collider.y - halfH;
        const float maxY = collider.y + halfH;
        if (p.x < minX || p.x > maxX || p.y < minY || p.y > maxY) {
            return;
        }
        const float distLeft = p.x - minX;
        const float distRight = maxX - p.x;
        const float distTop = p.y - minY;
        const float distBottom = maxY - p.y;
        const float minDist = std::min({distLeft, distRight, distTop, distBottom});
        if (minDist == distLeft) p.x = minX;
        else if (minDist == distRight) p.x = maxX;
        else if (minDist == distTop) p.y = minY;
        else p.y = maxY;
        p.prevX = p.x + (p.prevX - p.x) * (1.0f - collider.friction * collisionDamping_);
        p.prevY = p.y + (p.prevY - p.y) * collider.restitution;
    }

    void resolveCircle(SoftBodyPoint& p, const SoftBodyCollider& collider) {
        const float radius = std::max(0.0f, collider.radius);
        const float dx = p.x - collider.x;
        const float dy = p.y - collider.y;
        const float distSq = dx * dx + dy * dy;
        if (distSq >= radius * radius || distSq <= 1e-8f) {
            return;
        }
        const float dist = std::sqrt(distSq);
        const float nx = dx / dist;
        const float ny = dy / dist;
        p.x = collider.x + nx * radius;
        p.y = collider.y + ny * radius;
        p.prevX = p.x + (p.prevX - p.x) * (1.0f - collider.friction * collisionDamping_);
        p.prevY = p.y + (p.prevY - p.y) * collider.restitution;
    }

    std::vector<SoftBodyPoint> points_;
    std::vector<SoftBodyConstraint> constraints_;
    std::vector<SoftBodyCollider> colliders_;
    std::vector<SoftBodyVolumeTriangle> volumeTriangles_;
    float gravityX_ = 0.0f;
    float gravityY_ = 9.8f;
    float collisionDamping_ = 0.15f;
    int constraintIterations_ = 5;
    int collisionIterations_ = 2;
    bool tearingEnabled_ = false;
    float maxStrain_ = 2.0f;
    bool selfCollisionEnabled_ = false;
    float selfCollisionRadius_ = 5.0f;
    bool remeshingEnabled_ = false;
    float subdivideThreshold_ = 1.8f;
    float collapseThreshold_ = 0.3f;
    float minSegmentLength_ = 5.0f;
    float pressureStiffness_ = 0.0f;
    SoftBodyWind wind_;
    float turbulenceTime_ = 0.0f;
};

} // namespace ArtifactCore
