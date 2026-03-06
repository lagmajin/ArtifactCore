module;
#include <QVector3D>
#include <vector>
#include <memory>
#include <cmath>
#include <random>

export module ArtifactCore.Crowd.Boids;

import std;

export namespace ArtifactCore {

    // ─────────────────────────────────────────────────────────
    // BoidAgent
    // 群衆（Swarm）の1個体を表現する構造体
    // ─────────────────────────────────────────────────────────
    struct BoidAgent {
        int id = 0;
        QVector3D position;
        QVector3D velocity;
        QVector3D acceleration;
        float maxSpeed = 5.0f;
        float maxForce = 0.1f;
        float radius = 10.0f; // 衝突半径
        
        // ユーザー定義の拡張データ（MoGraphとの連携用）
        float scale = 1.0f;
        int typeIndex = 0;
    };

    // ─────────────────────────────────────────────────────────
    // BoidsSwarmSystem
    // クレイグ・レイノルズのBoidsアルゴリズムに基づく群衆シミュレーションコア
    // Separation (分離), Alignment (整列), Cohesion (結合) を計算する
    // ─────────────────────────────────────────────────────────
    class BoidsSwarmSystem {
    public:
        // シミュレーションパラメータ
        float separationWeight = 1.5f;
        float alignmentWeight = 1.0f;
        float cohesionWeight = 1.0f;
        float targetWeight = 1.0f;
        
        float neighborDistance = 50.0f;
        float desiredSeparation = 25.0f;

        QVector3D targetPosition;
        bool hasTarget = false;
        
        // 空間の境界 (0,0,0 を中心とするボックス)
        QVector3D bounds{1000.0f, 1000.0f, 1000.0f};

    private:
        std::vector<BoidAgent> agents_;

    public:
        BoidsSwarmSystem() = default;
        ~BoidsSwarmSystem() = default;

        // エージェントの追加
        void addAgent(const QVector3D& startPos, const QVector3D& initialVel) {
            BoidAgent boid;
            boid.id = static_cast<int>(agents_.size());
            boid.position = startPos;
            boid.velocity = initialVel;
            if (boid.velocity.lengthSquared() == 0) {
                boid.velocity = QVector3D(0, 0, 1.0f); // ゼロ割防止
            }
            agents_.push_back(boid);
        }

        // 初期化: 指定範囲にランダムなエージェントをN体生成
        void initializeRandom(int count, const QVector3D& volume, int seed = 123) {
            agents_.clear();
            agents_.reserve(count);
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> distX(-volume.x() / 2, volume.x() / 2);
            std::uniform_real_distribution<float> distY(-volume.y() / 2, volume.y() / 2);
            std::uniform_real_distribution<float> distZ(-volume.z() / 2, volume.z() / 2);
            
            std::uniform_real_distribution<float> distVel(-1.0f, 1.0f);

            for (int i = 0; i < count; ++i) {
                QVector3D pos(distX(rng), distY(rng), distZ(rng));
                QVector3D vel(distVel(rng), distVel(rng), distVel(rng));
                vel.normalize();
                vel *= 2.0f; // Initial speed
                addAgent(pos, vel);
            }
        }

        std::vector<BoidAgent>& getAgents() { return agents_; }
        const std::vector<BoidAgent>& getAgents() const { return agents_; }

        // フレーム更新 (deltaTime = 秒)
        void update(float deltaTime) {
            if (deltaTime <= 0.0f) return;

            // O(N^2) の素朴な実装。本来は空間分割（OctreeやGrid）が必要
            std::vector<QVector3D> newAccelerations(agents_.size(), QVector3D(0,0,0));

            for (size_t i = 0; i < agents_.size(); ++i) {
                const auto& boid = agents_[i];
                
                QVector3D sepForce(0,0,0);
                QVector3D aliForce(0,0,0);
                QVector3D cohForce(0,0,0);
                
                int sepCount = 0;
                int neighborCount = 0;

                for (size_t j = 0; j < agents_.size(); ++j) {
                    if (i == j) continue;
                    const auto& other = agents_[j];
                    float distSq = boid.position.distanceToPoint(other.position);
                    distSq = distSq * distSq; // 実際はdistanceToPointが長さを返すのでここでは単なるdist

                    float d = boid.position.distanceToPoint(other.position);

                    if (d > 0 && d < neighborDistance) {
                        // Alignment: 近隣の平均速度に向かう
                        aliForce += other.velocity;
                        
                        // Cohesion: 近隣の平均位置に向かう
                        cohForce += other.position;
                        neighborCount++;

                        // Separation: 近すぎる個体から離れる
                        if (d < desiredSeparation) {
                            QVector3D diff = boid.position - other.position;
                            diff.normalize();
                            diff /= d; // 距離に反比例して強く
                            sepForce += diff;
                            sepCount++;
                        }
                    }
                }

                // 計算されたフォースの正規化と重み付け
                if (neighborCount > 0) {
                    // Alignment
                    aliForce /= static_cast<float>(neighborCount);
                    aliForce.normalize();
                    aliForce *= boid.maxSpeed;
                    aliForce -= boid.velocity;
                    limitForce(aliForce, boid.maxForce);

                    // Cohesion
                    cohForce /= static_cast<float>(neighborCount);
                    cohForce = seek(boid, cohForce);
                }

                if (sepCount > 0) {
                    sepForce /= static_cast<float>(sepCount);
                    if (sepForce.lengthSquared() > 0) {
                        sepForce.normalize();
                        sepForce *= boid.maxSpeed;
                        sepForce -= boid.velocity;
                        limitForce(sepForce, boid.maxForce);
                    }
                }

                // Target Seeking (オプション)
                QVector3D tgtForce(0,0,0);
                if (hasTarget) {
                    tgtForce = seek(boid, targetPosition);
                }

                // 境界反射 (バウンディングボックス内に留める)
                QVector3D boundsForce = avoidBounds(boid);

                // 合計
                newAccelerations[i] += sepForce * separationWeight;
                newAccelerations[i] += aliForce * alignmentWeight;
                newAccelerations[i] += cohForce * cohesionWeight;
                newAccelerations[i] += tgtForce * targetWeight;
                newAccelerations[i] += boundsForce * 2.0f; // 境界は強めに
            }

            // 物理適用
            for (size_t i = 0; i < agents_.size(); ++i) {
                auto& boid = agents_[i];
                boid.acceleration = newAccelerations[i];
                
                boid.velocity += boid.acceleration;
                if (boid.velocity.length() > boid.maxSpeed) {
                    boid.velocity.normalize();
                    boid.velocity *= boid.maxSpeed;
                }
                boid.position += boid.velocity * (deltaTime * 60.0f); // 簡易的なスケール調整
                
                // 加速度リセット
                boid.acceleration = QVector3D(0,0,0);
            }
        }

    private:
        // ターゲットに向かう力を計算
        QVector3D seek(const BoidAgent& boid, const QVector3D& target) const {
            QVector3D desired = target - boid.position;
            float d = desired.length();
            if (d == 0) return QVector3D(0,0,0);
            
            desired.normalize();
            desired *= boid.maxSpeed;
            
            QVector3D steer = desired - boid.velocity;
            limitForce(steer, boid.maxForce);
            return steer;
        }

        // ベクトルの長さを制限する
        void limitForce(QVector3D& v, float max) const {
            if (v.lengthSquared() > max * max) {
                v.normalize();
                v *= max;
            }
        }

        // 境界(ボックス)に近づいたら反発する力
        QVector3D avoidBounds(const BoidAgent& boid) const {
            QVector3D force(0,0,0);
            float margin = 50.0f; // 境界から50単位近づいたら反発

            if (boid.position.x() < -bounds.x()/2 + margin) force.setX(boid.maxSpeed);
            else if (boid.position.x() > bounds.x()/2 - margin) force.setX(-boid.maxSpeed);

            if (boid.position.y() < -bounds.y()/2 + margin) force.setY(boid.maxSpeed);
            else if (boid.position.y() > bounds.y()/2 - margin) force.setY(-boid.maxSpeed);

            if (boid.position.z() < -bounds.z()/2 + margin) force.setZ(boid.maxSpeed);
            else if (boid.position.z() > bounds.z()/2 - margin) force.setZ(-boid.maxSpeed);

            return force;
        }
    };

}
