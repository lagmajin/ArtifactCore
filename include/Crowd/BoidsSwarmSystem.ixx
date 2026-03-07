module;
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module ArtifactCore.Crowd.Boids;




import Particle;
import Math.SpatialGrid;

export namespace ArtifactCore {

    /**
     * @brief BoidAgent using Particle as base for physics integration
     */
    struct BoidAgent : public Particle {
        float maxSpeed = 5.0f;
        float maxForce = 0.1f;
        float neighborDistance = 50.0f;
        float desiredSeparation = 25.0f;
    };

    /**
     * @brief Optimized Boids Swarm System using Spatial Grid
     */
    class BoidsSwarmSystem {
    public:
        // Simulation weights
        float separationWeight = 1.5f;
        float alignmentWeight = 1.0f;
        float cohesionWeight = 1.0f;
        float targetWeight = 1.0f;
        
        float3 targetPosition{0, 0, 0};
        bool hasTarget = false;
        
        // World boundaries
        float3 bounds{1000.0f, 1000.0f, 1000.0f};

    private:
        std::vector<BoidAgent> agents_;
        std::unique_ptr<SpatialGrid> grid_;

    public:
        BoidsSwarmSystem() : grid_(std::make_unique<SpatialGrid>(50.0f)) {}
        
        void addAgent(const float3& pos, const float3& vel) {
            BoidAgent boid;
            boid.position = pos;
            boid.velocity = vel;
            boid.id = static_cast<uint64_t>(agents_.size());
            agents_.push_back(boid);
        }

        void initializeRandom(int count, const float3& volume, int seed = 123) {
            agents_.clear();
            agents_.reserve(count);
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> distX(-volume.x / 2, volume.x / 2);
            std::uniform_real_distribution<float> distY(-volume.y / 2, volume.y / 2);
            std::uniform_real_distribution<float> distZ(-volume.z / 2, volume.z / 2);
            std::uniform_real_distribution<float> distVel(-1.0f, 1.0f);

            for (int i = 0; i < count; ++i) {
                float3 pos{distX(rng), distY(rng), distZ(rng)};
                float3 vel{distVel(rng), distVel(rng), distVel(rng)};
                float len = std::sqrt(vel.x*vel.x + vel.y*vel.y + vel.z*vel.z);
                if (len > 0) {
                    vel.x /= len; vel.y /= len; vel.z /= len;
                    vel.x *= 2.0f; vel.y *= 2.0f; vel.z *= 2.0f;
                }
                addAgent(pos, vel);
            }
        }

        /**
         * @brief Optimized update using Spatial Grid (O(N))
         */
        void update(float deltaTime) {
            if (deltaTime <= 0.0f || agents_.empty()) return;

            // Build spatial grid
            std::vector<float3> positions;
            positions.reserve(agents_.size());
            for (const auto& a : agents_) positions.push_back(a.position);
            grid_->build(positions);

            std::vector<float3> newAccelerations(agents_.size(), {0, 0, 0});

            for (size_t i = 0; i < agents_.size(); ++i) {
                const auto& boid = agents_[i];
                
                float3 sepForce{0,0,0}, aliForce{0,0,0}, cohForce{0,0,0};
                int sepCount = 0, neighborCount = 0;

                // Query neighbors from grid
                grid_->query(boid.position, boid.neighborDistance, [&](size_t j) {
                    if (i == j) return;
                    const auto& other = agents_[j];
                    
                    float3 diff{boid.position.x - other.position.x, 
                                boid.position.y - other.position.y, 
                                boid.position.z - other.position.z};
                    float d = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);

                    if (d > 0 && d < boid.neighborDistance) {
                        aliForce.x += other.velocity.x;
                        aliForce.y += other.velocity.y;
                        aliForce.z += other.velocity.z;
                        
                        cohForce.x += other.position.x;
                        cohForce.y += other.position.y;
                        cohForce.z += other.position.z;
                        neighborCount++;

                        if (d < boid.desiredSeparation) {
                            float factor = 1.0f / d;
                            sepForce.x += (diff.x / d) * factor;
                            sepForce.y += (diff.y / d) * factor;
                            sepForce.z += (diff.z / d) * factor;
                            sepCount++;
                        }
                    }
                });

                // Apply Boids Rules
                if (neighborCount > 0) {
                    float invCount = 1.0f / neighborCount;
                    // Alignment
                    aliForce.x *= invCount; aliForce.y *= invCount; aliForce.z *= invCount;
                    steer(aliForce, boid);
                    
                    // Cohesion
                    cohForce.x *= invCount; cohForce.y *= invCount; cohForce.z *= invCount;
                    cohForce = seek(boid, cohForce);
                }

                if (sepCount > 0) {
                    float invSep = 1.0f / sepCount;
                    sepForce.x *= invSep; sepForce.y *= invSep; sepForce.z *= invSep;
                    steer(sepForce, boid);
                }

                // Integration
                float3& acc = newAccelerations[i];
                acc.x += sepForce.x * separationWeight + aliForce.x * alignmentWeight + cohForce.x * cohesionWeight;
                acc.y += sepForce.y * separationWeight + aliForce.y * alignmentWeight + cohForce.y * cohesionWeight;
                acc.z += sepForce.z * separationWeight + aliForce.z * alignmentWeight + cohForce.z * cohesionWeight;

                // Bounds avoid
                float3 bForce = avoidBounds(boid);
                acc.x += bForce.x * 2.0f;
                acc.y += bForce.y * 2.0f;
                acc.z += bForce.z * 2.0f;
            }

            // Apply physics
            for (size_t i = 0; i < agents_.size(); ++i) {
                auto& boid = agents_[i];
                boid.acceleration = newAccelerations[i];
                
                boid.velocity.x += boid.acceleration.x;
                boid.velocity.y += boid.acceleration.y;
                boid.velocity.z += boid.acceleration.z;
                
                limit(boid.velocity, boid.maxSpeed);
                
                boid.position.x += boid.velocity.x * (deltaTime * 60.0f);
                boid.position.y += boid.velocity.y * (deltaTime * 60.0f);
                boid.position.z += boid.velocity.z * (deltaTime * 60.0f);
                
                boid.acceleration = {0, 0, 0};
            }
        }

        std::vector<BoidAgent>& getAgents() { return agents_; }

    private:
        void limit(float3& v, float max) const {
            float magSq = v.x*v.x + v.y*v.y + v.z*v.z;
            if (magSq > max * max) {
                float mag = std::sqrt(magSq);
                v.x /= mag; v.y /= mag; v.z /= mag;
                v.x *= max; v.y *= max; v.z *= max;
            }
        }

        void steer(float3& force, const BoidAgent& boid) const {
            float magSq = force.x*force.x + force.y*force.y + force.z*force.z;
            if (magSq > 0) {
                float mag = std::sqrt(magSq);
                force.x = (force.x / mag) * boid.maxSpeed - boid.velocity.x;
                force.y = (force.y / mag) * boid.maxSpeed - boid.velocity.y;
                force.z = (force.z / mag) * boid.maxSpeed - boid.velocity.z;
                limit(force, boid.maxForce);
            }
        }

        float3 seek(const BoidAgent& boid, const float3& target) const {
            float3 desired{target.x - boid.position.x, target.y - boid.position.y, target.z - boid.position.z};
            float magSq = desired.x*desired.x + desired.y*desired.y + desired.z*desired.z;
            if (magSq == 0) return {0,0,0};
            
            float mag = std::sqrt(magSq);
            desired.x = (desired.x / mag) * boid.maxSpeed - boid.velocity.x;
            desired.y = (desired.y / mag) * boid.maxSpeed - boid.velocity.y;
            desired.z = (desired.z / mag) * boid.maxSpeed - boid.velocity.z;
            limit(desired, boid.maxForce);
            return desired;
        }

        float3 avoidBounds(const BoidAgent& boid) const {
            float3 force{0,0,0};
            float margin = 50.0f;
            if (boid.position.x < -bounds.x/2 + margin) force.x = boid.maxSpeed;
            else if (boid.position.x > bounds.x/2 - margin) force.x = -boid.maxSpeed;
            if (boid.position.y < -bounds.y/2 + margin) force.y = boid.maxSpeed;
            else if (boid.position.y > bounds.y/2 - margin) force.y = -boid.maxSpeed;
            if (boid.position.z < -bounds.z/2 + margin) force.z = boid.maxSpeed;
            else if (boid.position.z > bounds.z/2 - margin) force.z = -boid.maxSpeed;
            return force;
        }
    };
}
