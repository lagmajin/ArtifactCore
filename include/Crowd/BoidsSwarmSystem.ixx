module;
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>
#include <deque>

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
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
import Graphics.ParticleData;

export namespace ArtifactCore {

    enum class AgentType : uint8_t {
        Normal = 0,
        Predator,
        Prey
    };

    enum class FormationType : uint8_t {
        None = 0,
        Vee,
        Line,
        Circle,
        Diamond
    };

    struct BoidAgent : public Particle {
        float maxSpeed = 5.0f;
        float maxForce = 0.1f;
        float neighborDistance = 50.0f;
        float desiredSeparation = 25.0f;
        AgentType type = AgentType::Normal;
        int groupId = 0;
        int formationSlot = -1;
        float panicTimer = 0.0f;
        int stuckFrames = 0;
        int boostCooldown = 0;
        int huntCooldown = 0;
    };

    struct Obstacle {
        float3 center;
        float radius = 50.0f;
    };

    struct FlowField {
        std::vector<float3> directions;
        float3 origin{-500, -500, -500};
        float3 size{1000, 1000, 1000};
        int resolutionX = 20;
        int resolutionY = 20;
        int resolutionZ = 20;

        float3 sample(float3 pos) const {
            float nx = (pos.x - origin.x) / size.x * static_cast<float>(resolutionX - 1);
            float ny = (pos.y - origin.y) / size.y * static_cast<float>(resolutionY - 1);
            float nz = (pos.z - origin.z) / size.z * static_cast<float>(resolutionZ - 1);
            int ix = std::clamp(static_cast<int>(nx), 0, resolutionX - 1);
            int iy = std::clamp(static_cast<int>(ny), 0, resolutionY - 1);
            int iz = std::clamp(static_cast<int>(nz), 0, resolutionZ - 1);
            size_t idx = static_cast<size_t>(iz) * resolutionY * resolutionX
                       + static_cast<size_t>(iy) * resolutionX
                       + static_cast<size_t>(ix);
            if (idx >= directions.size()) return {0,0,0};
            return directions[idx];
        }
    };

    struct TrailPoint {
        float3 position;
        float age = 0.0f;
    };

    struct DensitySample {
        float3 position;
        float density = 0.0f;
    };

    struct AgentDebugInfo {
        float3 separationForce{0,0,0};
        float3 alignmentForce{0,0,0};
        float3 cohesionForce{0,0,0};
        float3 targetForce{0,0,0};
        float3 obstacleForce{0,0,0};
        float3 wanderForce{0,0,0};
        float3 waypointForce{0,0,0};
        float3 formationForce{0,0,0};
        float3 flowForce{0,0,0};
        int neighborCount = 0;
    };

    struct GroupWeights {
        float separation = 1.5f;
        float alignment = 1.0f;
        float cohesion = 1.0f;
    };

    class BoidsSwarmSystem {
    public:
        // -- Standard boids weights --
        float separationWeight = 1.5f;
        float alignmentWeight = 1.0f;
        float cohesionWeight = 1.0f;
        float targetWeight = 1.0f;
        float wanderWeight = 0.5f;
        float obstacleWeight = 3.0f;
        float predatorWeight = 2.0f;
        float preyWeight = 2.0f;

        float3 targetPosition{0, 0, 0};
        bool hasTarget = false;

        float wanderRadius = 30.0f;
        float wanderDistance = 60.0f;
        float wanderJitter = 2.0f;

        // -- Predator/Prey --
        float predatorMaxSpeed = 7.0f;
        float predatorMaxForce = 0.2f;
        float predatorChaseRange = 200.0f;
        float preyMaxSpeed = 4.0f;
        float preyMaxForce = 0.15f;
        float preyFleeRange = 150.0f;
        float preySeparationBoost = 2.0f;

        float3 bounds{1000.0f, 1000.0f, 1000.0f};

        // -- Waypoint following --
        float waypointWeight = 2.0f;
        float waypointReachedDistance = 30.0f;

        // -- Formation control --
        FormationType formationType = FormationType::None;
        float formationWeight = 1.5f;
        float formationSpacing = 40.0f;

        // -- Flow field --
        float flowFieldWeight = 1.0f;

        // -- Leader --
        bool leaderEnabled = true;
        float leaderFollowWeight = 2.0f;

        // -- Panic scatter --
        float panicRange = 80.0f;
        float panicStrength = 5.0f;
        float panicDurationSec = 1.0f;

        // -- Stuck detection --
        float stuckSpeedThreshold = 0.5f;
        int stuckFramesThreshold = 60;
        float stuckBoostStrength = 3.0f;
        int stuckBoostCooldownFrames = 120;

        // -- Spawn/Despawn --
        float spawnRate = 0.0f;
        float despawnMargin = 100.0f;
        bool wrapAtBounds = true;

        // -- Spawn type distribution --
        float spawnNormalWeight = 1.0f;
        float spawnPredatorWeight = 0.1f;
        float spawnPreyWeight = 0.4f;

        // -- Predation --
        float catchRange = 15.0f;
        bool preyDespawnOnCatch = true;
        int huntCooldownFrames = 60;

        // -- Trail --
        int maxTrailLength = 20;

        // -- Density heatmap --
        bool densityHeatmapEnabled = false;
        float densityCellSize = 50.0f;

        // -- Auto group split --
        int maxGroupSize = 50;

        // -- LOD --
        float3 cameraPosition{0, 0, 0};
        float lodThreshold1 = 300.0f;
        float lodThreshold2 = 600.0f;
        int minAgentCount = 20;
        int maxAgentCount = 500;

    private:
        std::vector<BoidAgent> agents_;
        std::unique_ptr<SpatialGrid> grid_;
        std::vector<Obstacle> obstacles_;
        std::mt19937 rng_{123};
        std::uniform_real_distribution<float> wanderDist_{-1.0f, 1.0f};

        // -- Waypoints --
        std::vector<float3> waypoints_;
        int currentWaypointIndex_ = 0;
        bool waypointsLooped_ = false;

        // -- Flow field --
        const FlowField* flowField_ = nullptr;

        // -- Trail --
        std::vector<std::deque<TrailPoint>> trails_;

        // -- LOD --
        std::vector<int> agentLod_;
        std::vector<int> lodSkipCounter_;
        int lodFrameCount_ = 0;

        // -- Leader --
        int leaderIndex_ = -1;

        // -- Spawn --
        float spawnAccum_ = 0.0f;

        // -- Group weights --
        std::unordered_map<int, GroupWeights> groupWeights_;

    public:
        BoidsSwarmSystem() : grid_(std::make_unique<SpatialGrid>(50.0f)) {}

        // -- Agent management --

        void addAgent(const float3& pos, const float3& vel, AgentType type = AgentType::Normal) {
            BoidAgent boid;
            boid.position = pos;
            boid.velocity = vel;
            boid.id = static_cast<uint64_t>(agents_.size());
            boid.type = type;
            applyTypeDefaults(boid);
            agents_.push_back(boid);
            trails_.emplace_back();
            agentLod_.push_back(0);
            lodSkipCounter_.push_back(0);
        }

        void removeAgent(int index) {
            if (index < 0 || index >= static_cast<int>(agents_.size())) return;
            agents_.erase(agents_.begin() + index);
            if (index < static_cast<int>(trails_.size())) trails_.erase(trails_.begin() + index);
            if (index < static_cast<int>(agentLod_.size())) agentLod_.erase(agentLod_.begin() + index);
            if (index < static_cast<int>(lodSkipCounter_.size())) lodSkipCounter_.erase(lodSkipCounter_.begin() + index);
        }

        void initializeRandom(int count, const float3& volume, int seed = 123) {
            agents_.clear();
            trails_.clear();
            agentLod_.clear();
            lodSkipCounter_.clear();
            agents_.reserve(count);
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> distX(-volume.x / 2, volume.x / 2);
            std::uniform_real_distribution<float> distY(-volume.y / 2, volume.y / 2);
            std::uniform_real_distribution<float> distZ(-volume.z / 2, volume.z / 2);
            std::uniform_real_distribution<float> distVel(-1.0f, 1.0f);
            std::uniform_real_distribution<float> distType(0.0f, 1.0f);

            for (int i = 0; i < count; ++i) {
                float3 pos{distX(rng), distY(rng), distZ(rng)};
                float3 vel{distVel(rng), distVel(rng), distVel(rng)};
                float len = std::sqrt(vel.x*vel.x + vel.y*vel.y + vel.z*vel.z);
                if (len > 0) {
                    vel.x /= len; vel.y /= len; vel.z /= len;
                    vel.x *= 2.0f; vel.y *= 2.0f; vel.z *= 2.0f;
                }
                AgentType type = AgentType::Normal;
                float roll = distType(rng);
                if (roll < 0.1f) type = AgentType::Predator;
                else if (roll < 0.25f) type = AgentType::Prey;
                addAgent(pos, vel, type);
            }
        }

        void initializeWithTypes(int count, const float3& volume, int predatorCount = 10, int preyCount = 30, int seed = 123) {
            agents_.clear();
            trails_.clear();
            agentLod_.clear();
            lodSkipCounter_.clear();
            agents_.reserve(count);
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> distX(-volume.x / 2, volume.x / 2);
            std::uniform_real_distribution<float> distY(-volume.y / 2, volume.y / 2);
            std::uniform_real_distribution<float> distZ(-volume.z / 2, volume.z / 2);
            std::uniform_real_distribution<float> distVel(-1.0f, 1.0f);

            auto makeAgent = [&](AgentType t) {
                float3 pos{distX(rng), distY(rng), distZ(rng)};
                float3 vel{distVel(rng), distVel(rng), distVel(rng)};
                float len = std::sqrt(vel.x*vel.x + vel.y*vel.y + vel.z*vel.z);
                if (len > 0) {
                    vel.x /= len; vel.y /= len; vel.z /= len;
                    vel.x *= 2.0f; vel.y *= 2.0f; vel.z *= 2.0f;
                }
                addAgent(pos, vel, t);
            };

            for (int i = 0; i < predatorCount; ++i) makeAgent(AgentType::Predator);
            for (int i = 0; i < preyCount; ++i) makeAgent(AgentType::Prey);
            int normal = count - predatorCount - preyCount;
            for (int i = 0; i < normal; ++i) makeAgent(AgentType::Normal);
        }

        void initializeFormation(int count, const float3& center, float radius, FormationType formation = FormationType::Circle) {
            agents_.clear();
            trails_.clear();
            agents_.reserve(count);
            formationType = formation;
            for (int i = 0; i < count; ++i) {
                float angle = 6.283185f * static_cast<float>(i) / static_cast<float>(count);
                float3 pos{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle), center.z};
                float3 vel{0, 0, 0};
                BoidAgent boid;
                boid.position = pos;
                boid.velocity = vel;
                boid.id = static_cast<uint64_t>(agents_.size());
                boid.type = AgentType::Normal;
                boid.formationSlot = i;
                applyTypeDefaults(boid);
                agents_.push_back(boid);
                trails_.emplace_back();
            }
        }

        // -- Target --

        void setTarget(const float3& pos) {
            targetPosition = pos;
            hasTarget = true;
        }

        void clearTarget() {
            hasTarget = false;
        }

        // -- Waypoints --

        void setWaypoints(const std::vector<float3>& waypoints, bool looped = false) {
            waypoints_ = waypoints;
            currentWaypointIndex_ = 0;
            waypointsLooped_ = looped;
        }

        void clearWaypoints() {
            waypoints_.clear();
            currentWaypointIndex_ = 0;
        }

        const std::vector<float3>& getWaypoints() const { return waypoints_; }
        int currentWaypointIndex() const { return currentWaypointIndex_; }

        // -- Obstacles --

        void addObstacle(const float3& center, float radius) {
            obstacles_.push_back({center, radius});
        }

        void clearObstacles() {
            obstacles_.clear();
        }

        const std::vector<Obstacle>& getObstacles() const { return obstacles_; }

        // -- Flow field --

        void setFlowField(const FlowField* field) { flowField_ = field; }
        void clearFlowField() { flowField_ = nullptr; }
        bool hasFlowField() const { return flowField_ != nullptr; }

        // -- LOD --

        void setCameraPosition(const float3& pos) { cameraPosition = pos; }

        // -- Group split/merge --

        void setAgentGroup(size_t agentIndex, int groupId) {
            if (agentIndex < agents_.size()) agents_[agentIndex].groupId = groupId;
        }

        void setAllGroup(int groupId) {
            for (auto& a : agents_) a.groupId = groupId;
        }

        void splitGroup(int sourceGroupId, int newGroupId, float splitRatio = 0.5f) {
            int count = 0;
            int total = 0;
            for (auto& a : agents_) {
                if (a.groupId == sourceGroupId) total++;
            }
            int target = static_cast<int>(total * splitRatio);
            for (auto& a : agents_) {
                if (a.groupId == sourceGroupId && count < target) {
                    a.groupId = newGroupId;
                    count++;
                }
            }
        }

        void mergeGroups(int groupIdA, int groupIdB) {
            for (auto& a : agents_) {
                if (a.groupId == groupIdB) a.groupId = groupIdA;
            }
        }

        int groupCount(int groupId) const {
            int c = 0;
            for (auto& a : agents_) if (a.groupId == groupId) c++;
            return c;
        }

        int getLeaderIndex() const { return leaderIndex_; }

        // -- Per-group weights --
        void setGroupWeights(int groupId, const GroupWeights& w) { groupWeights_[groupId] = w; }
        void clearGroupWeights(int groupId) { groupWeights_.erase(groupId); }
        void clearAllGroupWeights() { groupWeights_.clear(); }

        // -- Core update --

        void update(float deltaTime) {
            if (deltaTime <= 0.0f || agents_.empty()) return;

            // LOD: assign levels and decide which agents to skip
            lodFrameCount_++;
            for (size_t i = 0; i < agents_.size(); ++i) {
                float dx = agents_[i].position.x - cameraPosition.x;
                float dy = agents_[i].position.y - cameraPosition.y;
                float dz = agents_[i].position.z - cameraPosition.z;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                int newLod = 0;
                if (dist > lodThreshold2) newLod = 2;
                else if (dist > lodThreshold1) newLod = 1;
                agentLod_[i] = newLod;

                int skipFrames = (newLod == 2) ? 4 : (newLod == 1) ? 2 : 1;
                lodSkipCounter_[i]--;
                if (lodSkipCounter_[i] <= 0) lodSkipCounter_[i] = 0;
            }

            std::vector<float3> positions;
            positions.reserve(agents_.size());
            for (const auto& a : agents_) positions.push_back(a.position);
            grid_->build(positions);

            // Advance waypoint if applicable
            if (!waypoints_.empty()) {
                float avgX = 0, avgY = 0, avgZ = 0;
                size_t n = std::min<size_t>(agents_.size(), 20);
                for (size_t i = 0; i < n; ++i) {
                    avgX += agents_[i].position.x;
                    avgY += agents_[i].position.y;
                    avgZ += agents_[i].position.z;
                }
                if (n > 0) {
                    avgX /= n; avgY /= n; avgZ /= n;
                    float3 toWp{waypoints_[currentWaypointIndex_].x - avgX,
                                waypoints_[currentWaypointIndex_].y - avgY,
                                waypoints_[currentWaypointIndex_].z - avgZ};
                    float dist = std::sqrt(toWp.x*toWp.x + toWp.y*toWp.y + toWp.z*toWp.z);
                    if (dist < waypointReachedDistance) {
                        currentWaypointIndex_++;
                        if (currentWaypointIndex_ >= static_cast<int>(waypoints_.size())) {
                            if (waypointsLooped_) currentWaypointIndex_ = 0;
                            else currentWaypointIndex_ = static_cast<int>(waypoints_.size()) - 1;
                        }
                    }
                }
            }

            // Leader election: find agent farthest in avg velocity direction
            leaderIndex_ = -1;
            if (leaderEnabled && !agents_.empty()) {
                float3 avgVel{0,0,0};
                for (const auto& a : agents_) {
                    avgVel.x += a.velocity.x;
                    avgVel.y += a.velocity.y;
                    avgVel.z += a.velocity.z;
                }
                float invN = 1.0f / static_cast<float>(agents_.size());
                avgVel.x *= invN; avgVel.y *= invN; avgVel.z *= invN;
                float velMag = std::sqrt(avgVel.x*avgVel.x + avgVel.y*avgVel.y + avgVel.z*avgVel.z);
                if (velMag > 0.1f) {
                    float3 dir{avgVel.x/velMag, avgVel.y/velMag, avgVel.z/velMag};
                    float bestScore = -1e9f;
                    for (size_t i = 0; i < agents_.size(); ++i) {
                        float dot = agents_[i].position.x*dir.x + agents_[i].position.y*dir.y + agents_[i].position.z*dir.z;
                        if (dot > bestScore) { bestScore = dot; leaderIndex_ = static_cast<int>(i); }
                    }
                }
            }

            std::vector<float3> newAccelerations(agents_.size(), {0, 0, 0});

            for (size_t i = 0; i < agents_.size(); ++i) {
                auto& boid = agents_[i];

                // LOD: skip force computation for distant agents
                bool skipForce = (lodSkipCounter_[i] > 0 && agentLod_[i] > 0);
                if (skipForce) {
                    newAccelerations[i] = boid.acceleration;
                    continue;
                }
                if (lodSkipCounter_[i] <= 0 && agentLod_[i] > 0) {
                    lodSkipCounter_[i] = (agentLod_[i] == 2) ? 4 : 2;
                }

                float3 sepForce{0,0,0}, aliForce{0,0,0}, cohForce{0,0,0};
                float3 predForce{0,0,0}, preyForce{0,0,0};
                int sepCount = 0, neighborCount = 0;
                int predCount = 0, preyCount = 0;

                float effSepWeight = separationWeight;
                float effAliWeight = alignmentWeight;
                float effCohWeight = cohesionWeight;
                if (boid.type == AgentType::Prey) effSepWeight *= preySeparationBoost;
                auto gwIt = groupWeights_.find(boid.groupId);
                if (gwIt != groupWeights_.end()) {
                    effSepWeight = gwIt->second.separation;
                    effAliWeight = gwIt->second.alignment;
                    effCohWeight = gwIt->second.cohesion;
                }

                // LOD: reduce neighbor distance for distant agents
                float effNeighborDist = boid.neighborDistance;
                if (agentLod_[i] == 1) effNeighborDist *= 0.5f;
                else if (agentLod_[i] == 2) effNeighborDist *= 0.25f;

                grid_->query(boid.position, effNeighborDist, [&](size_t j) {
                    if (i == j) return;
                    const auto& other = agents_[j];

                    // Group isolation: different groups don't interact
                    if (boid.groupId != other.groupId) return;

                    float3 diff{boid.position.x - other.position.x,
                                boid.position.y - other.position.y,
                                boid.position.z - other.position.z};
                    float d = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);

                    float effectiveRange = boid.neighborDistance;
                    if (boid.type == AgentType::Predator && other.type == AgentType::Prey) {
                        effectiveRange = predatorChaseRange;
                    } else if (boid.type == AgentType::Prey && other.type == AgentType::Predator) {
                        effectiveRange = preyFleeRange;
                    }

                    if (d > 0 && d < effectiveRange) {
                        if (boid.type == AgentType::Predator && other.type == AgentType::Prey) {
                            float3 toPrey{other.position.x - boid.position.x,
                                          other.position.y - boid.position.y,
                                          other.position.z - boid.position.z};
                            float preyDist = std::sqrt(toPrey.x*toPrey.x + toPrey.y*toPrey.y + toPrey.z*toPrey.z);
                            if (preyDist > 0) {
                                float inv = 1.0f / preyDist;
                                predForce.x += toPrey.x * inv;
                                predForce.y += toPrey.y * inv;
                                predForce.z += toPrey.z * inv;
                                predCount++;
                            }
                        } else if (boid.type == AgentType::Prey && other.type == AgentType::Predator) {
                            float3 fromPred{boid.position.x - other.position.x,
                                            boid.position.y - other.position.y,
                                            boid.position.z - other.position.z};
                            float predDist = std::sqrt(fromPred.x*fromPred.x + fromPred.y*fromPred.y + fromPred.z*fromPred.z);
                            if (predDist > 0) {
                                float inv = 1.0f / predDist;
                                preyForce.x += fromPred.x * inv;
                                preyForce.y += fromPred.y * inv;
                                preyForce.z += fromPred.z * inv;
                                preyCount++;
                                // Panic scatter trigger
                                if (predDist < panicRange) {
                                    boid.panicTimer = panicDurationSec;
                                }
                            }
                        }

                        if (other.type == boid.type || (boid.type == AgentType::Normal && other.type == AgentType::Normal)) {
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
                    }
                });

                if (neighborCount > 0) {
                    float invCount = 1.0f / neighborCount;
                    aliForce.x *= invCount; aliForce.y *= invCount; aliForce.z *= invCount;
                    steer(aliForce, boid);
                    cohForce.x *= invCount; cohForce.y *= invCount; cohForce.z *= invCount;
                    cohForce = seek(boid, cohForce);
                }
                if (sepCount > 0) {
                    float invSep = 1.0f / sepCount;
                    sepForce.x *= invSep; sepForce.y *= invSep; sepForce.z *= invSep;
                    steer(sepForce, boid);
                }

                if (predCount > 0 && boid.type == AgentType::Predator) {
                    float invPred = 1.0f / predCount;
                    predForce.x *= invPred; predForce.y *= invPred; predForce.z *= invPred;
                    steer(predForce, boid);
                }
                if (preyCount > 0 && boid.type == AgentType::Prey) {
                    float invPrey = 1.0f / preyCount;
                    preyForce.x *= invPrey; preyForce.y *= invPrey; preyForce.z *= invPrey;
                    steer(preyForce, boid);
                }

                // Obstacle avoidance
                float3 obsForce = avoidObstacles(boid);

                // Target or wander
                float3 tgtForce{0,0,0};
                float3 wndForce{0,0,0};
                if (hasTarget) {
                    tgtForce = seek(boid, targetPosition);
                } else if (waypoints_.empty()) {
                    wndForce = wander(boid);
                }

                // Waypoint following
                float3 wpForce{0,0,0};
                if (!waypoints_.empty()) {
                    wpForce = seek(boid, waypoints_[currentWaypointIndex_]);
                }

                // Formation
                float3 formationForce{0,0,0};
                if (formationType != FormationType::None && boid.formationSlot >= 0) {
                    formationForce = computeFormationForce(boid);
                }

                // Flow field
                float3 flowForce{0,0,0};
                if (flowField_) {
                    float3 dir = flowField_->sample(boid.position);
                    flowForce = seek(boid, {boid.position.x + dir.x * 100,
                                            boid.position.y + dir.y * 100,
                                            boid.position.z + dir.z * 100});
                }

                float3& acc = newAccelerations[i];
                acc.x += sepForce.x * effSepWeight + aliForce.x * effAliWeight + cohForce.x * effCohWeight;
                acc.y += sepForce.y * effSepWeight + aliForce.y * effAliWeight + cohForce.y * effCohWeight;
                acc.z += sepForce.z * effSepWeight + aliForce.z * effAliWeight + cohForce.z * effCohWeight;
                acc.x += predForce.x * predatorWeight;
                acc.y += predForce.y * predatorWeight;
                acc.z += predForce.z * predatorWeight;
                acc.x += preyForce.x * preyWeight;
                acc.y += preyForce.y * preyWeight;
                acc.z += preyForce.z * preyWeight;
                acc.x += obsForce.x * obstacleWeight;
                acc.y += obsForce.y * obstacleWeight;
                acc.z += obsForce.z * obstacleWeight;
                acc.x += tgtForce.x * targetWeight;
                acc.y += tgtForce.y * targetWeight;
                acc.z += tgtForce.z * targetWeight;
                acc.x += wndForce.x * wanderWeight;
                acc.y += wndForce.y * wanderWeight;
                acc.z += wndForce.z * wanderWeight;
                acc.x += wpForce.x * waypointWeight;
                acc.y += wpForce.y * waypointWeight;
                acc.z += wpForce.z * waypointWeight;
                acc.x += formationForce.x * formationWeight;
                acc.y += formationForce.y * formationWeight;
                acc.z += formationForce.z * formationWeight;
                acc.x += flowForce.x * flowFieldWeight;
                acc.y += flowForce.y * flowFieldWeight;
                acc.z += flowForce.z * flowFieldWeight;

                // Leader follow
                if (leaderEnabled && leaderIndex_ >= 0 && static_cast<size_t>(leaderIndex_) < agents_.size() && static_cast<size_t>(leaderIndex_) != i) {
                    float3 leaderPos = agents_[leaderIndex_].position;
                    float3 toLeader = seek(boid, leaderPos);
                    acc.x += toLeader.x * leaderFollowWeight;
                    acc.y += toLeader.y * leaderFollowWeight;
                    acc.z += toLeader.z * leaderFollowWeight;
                }

                // Panic scatter boost
                if (boid.panicTimer > 0.0f) {
                    float2 rnd{wanderDist_(rng_), wanderDist_(rng_)};
                    float3 scatter{rnd.x, rnd.y, wanderDist_(rng_)};
                    float sm = std::sqrt(scatter.x*scatter.x + scatter.y*scatter.y + scatter.z*scatter.z);
                    if (sm > 0) { scatter.x /= sm; scatter.y /= sm; scatter.z /= sm; }
                    acc.x += scatter.x * panicStrength;
                    acc.y += scatter.y * panicStrength;
                    acc.z += scatter.z * panicStrength;
                    boid.panicTimer -= deltaTime;
                }

                float3 bForce = avoidBounds(boid);
                acc.x += bForce.x * 2.0f;
                acc.y += bForce.y * 2.0f;
                acc.z += bForce.z * 2.0f;
            }

            for (size_t i = 0; i < agents_.size(); ++i) {
                auto& boid = agents_[i];
                boid.acceleration = newAccelerations[i];

                float effMaxSpeed = boid.maxSpeed;
                float effMaxForce = boid.maxForce;
                if (boid.type == AgentType::Predator) {
                    effMaxSpeed = predatorMaxSpeed;
                    effMaxForce = predatorMaxForce;
                } else if (boid.type == AgentType::Prey) {
                    effMaxSpeed = preyMaxSpeed;
                    effMaxForce = preyMaxForce;
                }

                boid.velocity.x += boid.acceleration.x;
                boid.velocity.y += boid.acceleration.y;
                boid.velocity.z += boid.acceleration.z;

                limit(boid.velocity, effMaxSpeed);

                boid.position.x += boid.velocity.x * (deltaTime * 60.0f);
                boid.position.y += boid.velocity.y * (deltaTime * 60.0f);
                boid.position.z += boid.velocity.z * (deltaTime * 60.0f);

                boid.acceleration = {0, 0, 0};

                // Update trail
                if (maxTrailLength > 0 && i < trails_.size()) {
                    auto& trail = trails_[i];
                    trail.push_front({boid.position, 0});
                    while (static_cast<int>(trail.size()) > maxTrailLength) {
                        trail.pop_back();
                    }
                    for (auto& tp : trail) tp.age += deltaTime;
                }

                // Stuck detection
                float speed = std::sqrt(boid.velocity.x*boid.velocity.x + boid.velocity.y*boid.velocity.y + boid.velocity.z*boid.velocity.z);
                if (speed < stuckSpeedThreshold) {
                    boid.stuckFrames++;
                    if (boid.stuckFrames > stuckFramesThreshold && boid.boostCooldown <= 0) {
                        float3 boost{wanderDist_(rng_), wanderDist_(rng_), wanderDist_(rng_)};
                        float bm = std::sqrt(boost.x*boost.x + boost.y*boost.y + boost.z*boost.z);
                        if (bm > 0) { boost.x /= bm; boost.y /= bm; boost.z /= bm; }
                        boid.velocity.x += boost.x * stuckBoostStrength;
                        boid.velocity.y += boost.y * stuckBoostStrength;
                        boid.velocity.z += boost.z * stuckBoostStrength;
                        boid.stuckFrames = 0;
                        boid.boostCooldown = stuckBoostCooldownFrames;
                    }
                } else {
                    boid.stuckFrames = 0;
                }
                if (boid.boostCooldown > 0) boid.boostCooldown--;
                if (boid.huntCooldown > 0) boid.huntCooldown--;
            }

            // Predation catch
            if (preyDespawnOnCatch && catchRange > 0.0f) {
                for (size_t pi = 0; pi < agents_.size(); ++pi) {
                    auto& pred = agents_[pi];
                    if (pred.type != AgentType::Predator) continue;
                    if (pred.huntCooldown > 0) continue;
                    for (size_t pj = 0; pj < agents_.size(); ++pj) {
                        if (pi == pj) continue;
                        auto& prey = agents_[pj];
                        if (prey.type != AgentType::Prey) continue;
                        float dx = prey.position.x - pred.position.x;
                        float dy = prey.position.y - pred.position.y;
                        float dz = prey.position.z - pred.position.z;
                        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist < catchRange) {
                            removeAgent(static_cast<int>(pj));
                            pred.huntCooldown = huntCooldownFrames;
                            if (pj < pi) pi--; // adjust index after removal
                            break;
                        }
                    }
                }
            }

            // Spawn/Despawn
            if (spawnRate > 0.0f) {
                spawnAccum_ += spawnRate * deltaTime;
                while (spawnAccum_ >= 1.0f) {
                    spawnAccum_ -= 1.0f;
                    float3 randPos{wanderDist_(rng_) * bounds.x * 0.4f,
                                   wanderDist_(rng_) * bounds.y * 0.4f,
                                   wanderDist_(rng_) * bounds.z * 0.4f};
                    float3 randVel{wanderDist_(rng_), wanderDist_(rng_), wanderDist_(rng_)};
                    float rl = std::sqrt(randVel.x*randVel.x + randVel.y*randVel.y + randVel.z*randVel.z);
                    if (rl > 0) { randVel.x /= rl; randVel.y /= rl; randVel.z /= rl; randVel.x *= 2.0f; randVel.y *= 2.0f; randVel.z *= 2.0f; }
                    AgentType spawnType = AgentType::Normal;
                    float totalW = spawnNormalWeight + spawnPredatorWeight + spawnPreyWeight;
                    if (totalW > 0) {
                        float roll = std::uniform_real_distribution<float>(0, totalW)(rng_);
                        if (roll < spawnNormalWeight) spawnType = AgentType::Normal;
                        else if (roll < spawnNormalWeight + spawnPredatorWeight) spawnType = AgentType::Predator;
                        else spawnType = AgentType::Prey;
                    }
                    addAgent(randPos, randVel, spawnType);
                }
            }
            if (wrapAtBounds) {
                float hx = bounds.x * 0.5f + despawnMargin;
                float hy = bounds.y * 0.5f + despawnMargin;
                float hz = bounds.z * 0.5f + despawnMargin;
                for (auto& a : agents_) {
                    if (a.position.x < -hx) a.position.x += bounds.x + despawnMargin * 2;
                    else if (a.position.x > hx) a.position.x -= bounds.x + despawnMargin * 2;
                    if (a.position.y < -hy) a.position.y += bounds.y + despawnMargin * 2;
                    else if (a.position.y > hy) a.position.y -= bounds.y + despawnMargin * 2;
                    if (a.position.z < -hz) a.position.z += bounds.z + despawnMargin * 2;
                    else if (a.position.z > hz) a.position.z -= bounds.z + despawnMargin * 2;
                }
            }

            // Auto group split
            if (maxGroupSize > 0) {
                std::unordered_map<int, int> groupCounts;
                for (const auto& a : agents_) groupCounts[a.groupId]++;
                int nextGroupId = 1;
                for (const auto& kv : groupCounts) {
                    if (!groupCounts.empty()) nextGroupId = std::max(nextGroupId, kv.first + 1);
                }
                for (const auto& kv : groupCounts) {
                    if (kv.second > maxGroupSize) {
                        int newId = nextGroupId++;
                        int half = kv.second / 2;
                        int assigned = 0;
                        for (auto& a : agents_) {
                            if (a.groupId == kv.first && assigned < half) {
                                a.groupId = newId;
                                assigned++;
                            }
                        }
                    }
                }
            }

            // LOD auto adjustment: maintain agent count within min/max bounds
            int totalAgents = static_cast<int>(agents_.size());
            if (totalAgents > maxAgentCount) {
                int toRemove = totalAgents - maxAgentCount;
                for (int r = 0; r < toRemove && !agents_.empty(); ++r) {
                    int furthestIdx = 0;
                    float maxDist = -1.0f;
                    for (size_t i = 0; i < agents_.size(); ++i) {
                        float dx = agents_[i].position.x - cameraPosition.x;
                        float dy = agents_[i].position.y - cameraPosition.y;
                        float dz = agents_[i].position.z - cameraPosition.z;
                        float d = dx*dx + dy*dy + dz*dz;
                        if (d > maxDist) { maxDist = d; furthestIdx = static_cast<int>(i); }
                    }
                    if (furthestIdx < static_cast<int>(agents_.size())) {
                        removeAgent(furthestIdx);
                    }
                }
            } else if (totalAgents < minAgentCount) {
                int toAdd = minAgentCount - totalAgents;
                for (int a = 0; a < toAdd; ++a) {
                    float3 randPos{wanderDist_(rng_) * bounds.x * 0.4f,
                                   wanderDist_(rng_) * bounds.y * 0.4f,
                                   wanderDist_(rng_) * bounds.z * 0.4f};
                    float3 randVel{wanderDist_(rng_), wanderDist_(rng_), wanderDist_(rng_)};
                    float rl = std::sqrt(randVel.x*randVel.x + randVel.y*randVel.y + randVel.z*randVel.z);
                    if (rl > 0) { randVel.x /= rl; randVel.y /= rl; randVel.z /= rl; randVel.x *= 2.0f; randVel.y *= 2.0f; randVel.z *= 2.0f; }
                    addAgent(randPos, randVel);
                }
            }
        }

        // -- Perlin noise flow field generation --

        static FlowField generatePerlinFlowField(const float3& origin, const float3& size,
                                                  int resX, int resY, int resZ,
                                                  float noiseScale = 0.01f, float amplitude = 100.0f,
                                                  int octaves = 3, int seed = 0) {
            FlowField field;
            field.origin = origin;
            field.size = size;
            field.resolutionX = resX;
            field.resolutionY = resY;
            field.resolutionZ = resZ;
            field.directions.resize(static_cast<size_t>(resX) * resY * resZ);

            std::mt19937 permRng(seed);
            std::vector<int> perm(512);
            {
                std::vector<int> p(256);
                for (int i = 0; i < 256; ++i) p[i] = i;
                std::shuffle(p.begin(), p.end(), permRng);
                for (int i = 0; i < 512; ++i) perm[i] = p[i & 255];
            }

            auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
            auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
            auto grad = [&](int hash, float x, float y, float z) {
                int h = hash & 15;
                float u = (h < 8) ? x : y;
                float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
                return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
            };

            auto noise = [&](float x, float y, float z) -> float {
                int X = static_cast<int>(std::floor(x)) & 255;
                int Y = static_cast<int>(std::floor(y)) & 255;
                int Z = static_cast<int>(std::floor(z)) & 255;
                x -= std::floor(x);
                y -= std::floor(y);
                z -= std::floor(z);
                float u = fade(x), v = fade(y), w = fade(z);
                int a = perm[X] + Y, aa = perm[a] + Z, ab = perm[a + 1] + Z;
                int b = perm[X + 1] + Y, ba = perm[b] + Z, bb = perm[b + 1] + Z;
                return lerp(
                    lerp(lerp(grad(perm[aa], x, y, z), grad(perm[ba], x - 1, y, z), u),
                         lerp(grad(perm[ab], x, y - 1, z), grad(perm[bb], x - 1, y - 1, z), u), v),
                    lerp(lerp(grad(perm[aa + 1], x, y, z - 1), grad(perm[ba + 1], x - 1, y, z - 1), u),
                         lerp(grad(perm[ab + 1], x, y - 1, z - 1), grad(perm[bb + 1], x - 1, y - 1, z - 1), u), v), w);
            };

            auto fbm = [&](float x, float y, float z) {
                float value = 0.0f, weight = 1.0f, totalWeight = 0.0f;
                for (int o = 0; o < octaves; ++o) {
                    value += noise(x * weight, y * weight, z * weight) * weight;
                    totalWeight += weight;
                    weight *= 0.5f;
                }
                return value / totalWeight;
            };

            for (int iz = 0; iz < resZ; ++iz) {
                for (int iy = 0; iy < resY; ++iy) {
                    for (int ix = 0; ix < resX; ++ix) {
                        float wx = origin.x + size.x * static_cast<float>(ix) / static_cast<float>(resX - 1);
                        float wy = origin.y + size.y * static_cast<float>(iy) / static_cast<float>(resY - 1);
                        float wz = origin.z + size.z * static_cast<float>(iz) / static_cast<float>(resZ - 1);

                        float sx = noiseScale, sy = noiseScale, sz = noiseScale;
                        float vx = fbm(wx * sx, wy * sy, wz * sz);
                        float vy = fbm(wx * sx + 100.0f, wy * sy, wz * sz);
                        float vz = fbm(wx * sx, wy * sy + 100.0f, wz * sz + 100.0f);

                        // Central difference gradient approximation
                        float dx = fbm((wx + 1.0f) * sx, wy * sy, wz * sz) - vx;
                        float dy = fbm(wx * sx, (wy + 1.0f) * sy, wz * sz) - vy;
                        float dz = fbm(wx * sx, wy * sy, (wz + 1.0f) * sz) - vz;

                        float3 dir{dx * amplitude, dy * amplitude, dz * amplitude};
                        float mag = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                        if (mag > 0.001f) { dir.x /= mag; dir.y /= mag; dir.z /= mag; }

                        size_t idx = static_cast<size_t>(iz) * resY * resX
                                   + static_cast<size_t>(iy) * resX
                                   + static_cast<size_t>(ix);
                        field.directions[idx] = dir;
                    }
                }
            }
            return field;
        }

        // -- Accessors --

        std::vector<BoidAgent>& getAgents() { return agents_; }
        const std::vector<BoidAgent>& getAgents() const { return agents_; }
        std::vector<std::deque<TrailPoint>>& getTrails() { return trails_; }
        const std::vector<std::deque<TrailPoint>>& getTrails() const { return trails_; }

        // -- Render data --

        ParticleRenderData captureRenderData(int64_t frameNumber = 0) const {
            ParticleRenderData data;
            data.frameNumber = frameNumber;
            data.particles.reserve(agents_.size());

            // Density heatmap lookup
            std::vector<DensitySample> heatmap;
            if (densityHeatmapEnabled) heatmap = computeDensityGrid(densityCellSize);
            auto densityAtPos = [&](const float3& pos) -> float {
                if (heatmap.empty()) return -1.0f;
                float bestDist = 1e9f;
                float bestDensity = 0.0f;
                for (const auto& s : heatmap) {
                    float dx = s.position.x - pos.x, dy = s.position.y - pos.y, dz = s.position.z - pos.z;
                    float d = dx*dx + dy*dy + dz*dz;
                    if (d < bestDist) { bestDist = d; bestDensity = s.density; }
                }
                return bestDensity;
            };

            for (const auto& boid : agents_) {
                ParticleVertex v;
                v.px = boid.position.x;
                v.py = boid.position.y;
                v.pz = boid.position.z;
                v.vx = boid.velocity.x;
                v.vy = boid.velocity.y;
                v.vz = boid.velocity.z;
                float r = boid.color.x, g = boid.color.y, b = boid.color.z, a = boid.color.w * boid.opacity;
                if (densityHeatmapEnabled) {
                    float density = densityAtPos(boid.position);
                    if (density >= 0.0f) {
                        // Blue -> Cyan -> Yellow -> Red gradient
                        float t = std::min(density, 1.0f);
                        if (t < 0.33f) {
                            float u = t / 0.33f;
                            r = 0.0f; g = u * 1.0f; b = 1.0f - u * 0.7f;
                        } else if (t < 0.66f) {
                            float u = (t - 0.33f) / 0.33f;
                            r = u * 1.0f; g = 1.0f; b = 1.0f - u;
                        } else {
                            float u = (t - 0.66f) / 0.34f;
                            r = 1.0f; g = 1.0f - u * 0.8f; b = 0.0f;
                        }
                        a = 1.0f;
                    }
                } else if (r == 0 && g == 0 && b == 0 && boid.color.w == 0) {
                    if (boid.type == AgentType::Predator) { r = 1.0f; g = 0.2f; b = 0.2f; }
                    else if (boid.type == AgentType::Prey) { r = 0.2f; g = 1.0f; b = 0.2f; }
                    else { r = 0.3f; g = 0.6f; b = 1.0f; }
                    a = 1.0f;
                }
                v.r = r; v.g = g; v.b = b; v.a = a;
                v.size = (boid.type == AgentType::Predator) ? boid.size * 1.5f : boid.size;
                v.stretch = 1.0f;
                v.rotation = 0.0f;
                v.age = boid.age;
                v.lifetime = boid.lifetime;
                v.spriteFrame = 0;
                v.spriteRows = 1;
                v.spriteCols = 1;
                data.particles.push_back(v);
            }
            return data;
        }

        ParticleRenderData captureTrailRenderData(int64_t frameNumber = 0) const {
            ParticleRenderData data;
            data.frameNumber = frameNumber;
            int totalPoints = 0;
            for (const auto& trail : trails_) totalPoints += static_cast<int>(trail.size());
            data.particles.reserve(totalPoints);
            for (size_t i = 0; i < agents_.size() && i < trails_.size(); ++i) {
                const auto& boid = agents_[i];
                const auto& trail = trails_[i];
                float r = 0.5f, g = 0.7f, b = 1.0f, a = 1.0f;
                if (boid.type == AgentType::Predator) { r = 1.0f; g = 0.3f; b = 0.3f; }
                else if (boid.type == AgentType::Prey) { r = 0.3f; g = 0.8f; b = 0.3f; }
                for (const auto& tp : trail) {
                    ParticleVertex v;
                    v.px = tp.position.x;
                    v.py = tp.position.y;
                    v.pz = tp.position.z;
                    v.vx = 0; v.vy = 0; v.vz = 0;
                    float fade = 1.0f - std::min(tp.age / std::max(1, maxTrailLength), 1.0f);
                    v.r = r * fade; v.g = g * fade; v.b = b * fade; v.a = fade * 0.5f;
                    v.size = boid.size * 0.5f * fade;
                    v.stretch = 1.0f;
                    v.rotation = 0.0f;
                    v.age = tp.age;
                    v.lifetime = static_cast<float>(maxTrailLength);
                    v.spriteFrame = 0;
                    v.spriteRows = 1;
                    v.spriteCols = 1;
                    data.particles.push_back(v);
                }
            }
            return data;
        }

        // -- Density heatmap --

        std::vector<DensitySample> computeDensityGrid(float cellSize = 50.0f) const {
            if (agents_.empty() || cellSize <= 0) return {};
            float minX = bounds.x * -0.5f, maxX = bounds.x * 0.5f;
            float minY = bounds.y * -0.5f, maxY = bounds.y * 0.5f;
            float minZ = bounds.z * -0.5f, maxZ = bounds.z * 0.5f;
            int nx = std::max(1, static_cast<int>((maxX - minX) / cellSize));
            int ny = std::max(1, static_cast<int>((maxY - minY) / cellSize));
            int nz = std::max(1, static_cast<int>((maxZ - minZ) / cellSize));
            std::vector<int> grid(nx * ny * nz, 0);
            for (const auto& a : agents_) {
                int ix = std::clamp(static_cast<int>((a.position.x - minX) / cellSize), 0, nx - 1);
                int iy = std::clamp(static_cast<int>((a.position.y - minY) / cellSize), 0, ny - 1);
                int iz = std::clamp(static_cast<int>((a.position.z - minZ) / cellSize), 0, nz - 1);
                grid[iz * ny * nx + iy * nx + ix]++;
            }
            float invMax = 0.0f;
            for (int v : grid) if (v > invMax) invMax = static_cast<float>(v);
            invMax = (invMax > 0) ? 1.0f / invMax : 1.0f;
            std::vector<DensitySample> samples;
            samples.reserve(grid.size());
            for (int iz = 0; iz < nz; ++iz) {
                for (int iy = 0; iy < ny; ++iy) {
                    for (int ix = 0; ix < nx; ++ix) {
                        float3 pos{minX + (ix + 0.5f) * cellSize,
                                   minY + (iy + 0.5f) * cellSize,
                                   minZ + (iz + 0.5f) * cellSize};
                        float density = static_cast<float>(grid[iz * ny * nx + iy * nx + ix]) * invMax;
                        samples.push_back({pos, density});
                    }
                }
            }
            return samples;
        }

    private:
        void applyTypeDefaults(BoidAgent& agent) {
            switch (agent.type) {
            case AgentType::Predator:
                agent.maxSpeed = predatorMaxSpeed;
                agent.maxForce = predatorMaxForce;
                agent.neighborDistance = 80.0f;
                agent.desiredSeparation = 30.0f;
                agent.size = 6.0f;
                break;
            case AgentType::Prey:
                agent.maxSpeed = preyMaxSpeed;
                agent.maxForce = preyMaxForce;
                agent.neighborDistance = 60.0f;
                agent.desiredSeparation = 35.0f;
                agent.size = 3.0f;
                break;
            default:
                agent.size = 4.0f;
                break;
            }
        }

        float3 computeFormationForce(const BoidAgent& boid) {
            // Compute average position of same-formation agents
            float3 center{0,0,0};
            int count = 0;
            for (const auto& other : agents_) {
                if (other.formationSlot >= 0) {
                    center.x += other.position.x;
                    center.y += other.position.y;
                    center.z += other.position.z;
                    count++;
                }
            }
            if (count == 0) return {0,0,0};
            float inv = 1.0f / count;
            center.x *= inv; center.y *= inv; center.z *= inv;

            // Compute centroid velocity direction
            float3 avgVel{0,0,0};
            for (const auto& other : agents_) {
                avgVel.x += other.velocity.x;
                avgVel.y += other.velocity.y;
                avgVel.z += other.velocity.z;
            }
            avgVel.x *= inv; avgVel.y *= inv; avgVel.z *= inv;
            float speed = std::sqrt(avgVel.x*avgVel.x + avgVel.y*avgVel.y + avgVel.z*avgVel.z);
            float3 fwd{avgVel.x, avgVel.y, avgVel.z};
            if (speed > 0.001f) {
                fwd.x /= speed; fwd.y /= speed; fwd.z /= speed;
            }

            // Perpendicular (right) vector
            float3 right = {fwd.y, -fwd.x, fwd.z};

            // Compute slot target offset
            float3 offset{0,0,0};
            int slot = boid.formationSlot;
            switch (formationType) {
            case FormationType::Vee: {
                int rank = slot / 2;
                int side = (slot % 2 == 0) ? -1 : 1;
                offset.x = fwd.x * -rank * formationSpacing + right.x * side * (rank + 1) * formationSpacing * 0.5f;
                offset.y = fwd.y * -rank * formationSpacing + right.y * side * (rank + 1) * formationSpacing * 0.5f;
                offset.z = fwd.z * -rank * formationSpacing;
                break;
            }
            case FormationType::Line: {
                int rank = slot / 2;
                offset.x = right.x * (slot - count/2) * formationSpacing;
                offset.y = right.y * (slot - count/2) * formationSpacing;
                offset.z = 0;
                break;
            }
            case FormationType::Circle: {
                float angle = 6.283185f * slot / std::max(1, count);
                offset.x = formationSpacing * std::cos(angle);
                offset.y = formationSpacing * std::sin(angle);
                offset.z = 0;
                break;
            }
            case FormationType::Diamond: {
                int perRow = 1;
                int row = 0;
                int acc = 0;
                while (acc + perRow <= slot) { acc += perRow; row++; perRow += (row % 2 == 0) ? 2 : 0; }
                int col = slot - acc;
                float cx = (col - perRow * 0.5f) * formationSpacing;
                offset.x = cx;
                offset.y = fwd.x * 0; // flat diamond
                offset.z = 0;
                break;
            }
            default: break;
            }

            float3 targetPos{center.x + offset.x, center.y + offset.y, center.z + offset.z};
            return seek(boid, targetPos);
        }

        float3 wander(BoidAgent& boid) {
            float angleX = wanderDist_(rng_) * wanderJitter;
            float angleY = wanderDist_(rng_) * wanderJitter;
            float angleZ = wanderDist_(rng_) * wanderJitter;

            float speed = std::sqrt(boid.velocity.x*boid.velocity.x +
                                    boid.velocity.y*boid.velocity.y +
                                    boid.velocity.z*boid.velocity.z);
            if (speed < 0.001f) return {wanderDist_(rng_), wanderDist_(rng_), wanderDist_(rng_)};

            float3 dir{boid.velocity.x / speed, boid.velocity.y / speed, boid.velocity.z / speed};
            float3 center{dir.x * wanderDistance, dir.y * wanderDistance, dir.z * wanderDistance};
            float wx = center.x + wanderRadius * angleX;
            float wy = center.y + wanderRadius * angleY;
            float wz = center.z + wanderRadius * angleZ;

            return seek(boid, {boid.position.x + wx, boid.position.y + wy, boid.position.z + wz});
        }

        float3 avoidObstacles(const BoidAgent& boid) const {
            float3 force{0,0,0};
            for (const auto& obs : obstacles_) {
                float3 diff{boid.position.x - obs.center.x,
                            boid.position.y - obs.center.y,
                            boid.position.z - obs.center.z};
                float dist = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
                float threatRange = obs.radius + boid.neighborDistance * 0.5f;
                if (dist < threatRange && dist > 0.001f) {
                    float strength = (threatRange - dist) / threatRange;
                    float inv = 1.0f / dist;
                    force.x += (diff.x * inv) * strength * boid.maxSpeed;
                    force.y += (diff.y * inv) * strength * boid.maxSpeed;
                    force.z += (diff.z * inv) * strength * boid.maxSpeed;
                }
            }
            return force;
        }

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
