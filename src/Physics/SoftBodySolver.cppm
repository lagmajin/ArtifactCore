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
};

/**
 * @brief 質点間の距離拘束（バネ）
 */
export struct SoftBodyConstraint {
    int p1Idx;
    int p2Idx;
    float restDistance;
    float stiffness = 1.0f;
};

/**
 * @brief Verlet 積分を用いたシンプルなソフトボディソルバー
 */
export class SoftBodySolver {
public:
    SoftBodySolver() = default;

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
     * @brief シミュレーションを 1 ステップ進める
     */
    void update(float dt, float gravityX, float gravityY, int iterations = 5) {
        if (points_.empty()) return;

        // 1. 積分（移動）
        for (auto& p : points_) {
            if (p.isPinned) continue;

            float vx = (p.x - p.prevX);
            float vy = (p.y - p.prevY);
            
            p.prevX = p.x;
            p.prevY = p.y;
            
            p.x += vx + gravityX * dt * dt;
            p.y += vy + gravityY * dt * dt;
        }

        // 2. 拘束解決（反復計算）
        for (int i = 0; i < iterations; ++i) {
            for (const auto& c : constraints_) {
                auto& p1 = points_[c.p1Idx];
                auto& p2 = points_[c.p2Idx];
                
                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float currentDist = std::sqrt(dx * dx + dy * dy);
                if (currentDist < 1e-6f) continue;
                
                float delta = (currentDist - c.restDistance) / currentDist;
                float forceX = dx * 0.5f * delta * c.stiffness;
                float forceY = dy * 0.5f * delta * c.stiffness;
                
                if (!p1.isPinned) {
                    p1.x += forceX;
                    p1.y += forceY;
                }
                if (!p2.isPinned) {
                    p2.x -= forceX;
                    p2.y -= forceY;
                }
            }
        }
    }

    void clear() {
        points_.clear();
        constraints_.clear();
    }

    const std::vector<SoftBodyPoint>& getPoints() const { return points_; }
    const std::vector<SoftBodyConstraint>& getConstraints() const { return constraints_; }

private:
    std::vector<SoftBodyPoint> points_;
    std::vector<SoftBodyConstraint> constraints_;
};

} // namespace ArtifactCore
