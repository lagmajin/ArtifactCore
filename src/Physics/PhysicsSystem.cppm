module;
#include <utility>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <memory>
#include <map>
#include <vector>
#include <QString>
#include <QVector2D>
#include <optional>

export module Physics.System;

import Physics.Fluid;
import Physics2D;
import Physics.SoftBody;
import Physics.Mpm2D;
import Core.Simulation.Pyro;
import Graphics.ParticleData;
import Graphics.BoidsCompute;
import Memory.TrackedPtr;
import Memory.SharedPtr;
import Utils.Id;
import Container.NamedVector;

namespace ArtifactCore {

export struct MaterialFractureEvent {
    LayerID layerId;
    int fracturedParticleCount = 0;
    int totalParticleCount = 0;
};

export enum class PhysicsLODLevel : std::uint8_t {
    Full = 0,
    Reduced = 1,
    Minimal = 2,
    Frozen = 3,
};

export struct PhysicsLODSettings {
    PhysicsLODLevel level = PhysicsLODLevel::Full;
    // 0 means update on every call. Positive values rate-limit simulation.
    float targetHz = 0.0f;
    // 0 means keep each solver's existing default.
    int rigidBodySubSteps = 0;
    int softBodyMaxSubSteps = 0;
    int softBodyConstraintIterations = 0;
    int softBodyCollisionIterations = 0;
    float softBodyGridScale = 1.0f;
    int fluidSolverIterations = 0;
    float fluidResolutionScale = 1.0f;
    int materialMaxSubSteps = 0;
    bool disableFracture = false;
    float fractureShardScale = 1.0f;
    float fractureDebrisScale = 1.0f;
    bool simplifyCollisionMesh = false;
    bool disableSoftBodySelfCollision = false;
    bool applySleepPolicy = false;
    float sleepThreshold = 0.0f;
    bool disableContinuousCollision = false;
};

/**
 * @brief 物理演算システム。コンポジション内のシミュレーションを統合管理する。
 * UIを持たない「Core」レイヤーでのシミュレーション実行を担う。
 */
export class PhysicsSystem {
public:
    static PhysicsSystem& instance() {
        static PhysicsSystem inst;
        return inst;
    }

    void setPhysicsLODSettings(const PhysicsLODSettings& settings) {
        lodSettings_ = settings;
        // Presets only fill unspecified values. Callers can override any
        // individual budget while keeping the selected LOD level.
        if (lodSettings_.level == PhysicsLODLevel::Reduced) {
            if (lodSettings_.targetHz <= 0.0f) lodSettings_.targetHz = 30.0f;
            if (lodSettings_.rigidBodySubSteps <= 0) lodSettings_.rigidBodySubSteps = 2;
            if (lodSettings_.softBodyMaxSubSteps <= 0) lodSettings_.softBodyMaxSubSteps = 4;
            if (lodSettings_.softBodyConstraintIterations <= 0) lodSettings_.softBodyConstraintIterations = 3;
            if (lodSettings_.softBodyCollisionIterations <= 0) lodSettings_.softBodyCollisionIterations = 1;
            if (lodSettings_.softBodyGridScale >= 1.0f) lodSettings_.softBodyGridScale = 0.75f;
            if (lodSettings_.fluidSolverIterations <= 0) lodSettings_.fluidSolverIterations = 10;
            if (lodSettings_.fluidResolutionScale >= 1.0f) lodSettings_.fluidResolutionScale = 0.5f;
            if (lodSettings_.materialMaxSubSteps <= 0) lodSettings_.materialMaxSubSteps = 256;
            lodSettings_.applySleepPolicy = true;
            if (lodSettings_.sleepThreshold <= 0.0f) lodSettings_.sleepThreshold = 0.5f;
        } else if (lodSettings_.level == PhysicsLODLevel::Minimal) {
            if (lodSettings_.targetHz <= 0.0f) lodSettings_.targetHz = 15.0f;
            if (lodSettings_.rigidBodySubSteps <= 0) lodSettings_.rigidBodySubSteps = 1;
            if (lodSettings_.softBodyMaxSubSteps <= 0) lodSettings_.softBodyMaxSubSteps = 2;
            if (lodSettings_.softBodyConstraintIterations <= 0) lodSettings_.softBodyConstraintIterations = 1;
            if (lodSettings_.softBodyCollisionIterations <= 0) lodSettings_.softBodyCollisionIterations = 1;
            if (lodSettings_.softBodyGridScale >= 1.0f) lodSettings_.softBodyGridScale = 0.5f;
            if (lodSettings_.fluidSolverIterations <= 0) lodSettings_.fluidSolverIterations = 5;
            if (lodSettings_.fluidResolutionScale >= 1.0f) lodSettings_.fluidResolutionScale = 0.25f;
            if (lodSettings_.materialMaxSubSteps <= 0) lodSettings_.materialMaxSubSteps = 128;
            lodSettings_.disableSoftBodySelfCollision = true;
            lodSettings_.applySleepPolicy = true;
            if (lodSettings_.sleepThreshold <= 0.0f) lodSettings_.sleepThreshold = 1.0f;
            lodSettings_.disableContinuousCollision = true;
            lodSettings_.simplifyCollisionMesh = true;
            lodSettings_.disableFracture = true;
            lodSettings_.fractureShardScale = 0.5f;
            lodSettings_.fractureDebrisScale = 0.25f;
        }
        lodSettings_.targetHz = std::max(0.0f, lodSettings_.targetHz);
        lodSettings_.rigidBodySubSteps = std::max(0, lodSettings_.rigidBodySubSteps);
        lodSettings_.softBodyMaxSubSteps = std::max(0, lodSettings_.softBodyMaxSubSteps);
        lodSettings_.softBodyConstraintIterations = std::max(0, lodSettings_.softBodyConstraintIterations);
        lodSettings_.softBodyCollisionIterations = std::max(0, lodSettings_.softBodyCollisionIterations);
        lodSettings_.softBodyGridScale = std::clamp(lodSettings_.softBodyGridScale, 0.25f, 1.0f);
        lodSettings_.fluidSolverIterations = std::max(0, lodSettings_.fluidSolverIterations);
        lodSettings_.fluidResolutionScale = std::clamp(lodSettings_.fluidResolutionScale, 0.125f, 1.0f);
        lodSettings_.materialMaxSubSteps = std::max(0, lodSettings_.materialMaxSubSteps);
        if (lodSettings_.targetHz <= 0.0f) {
            lodAccumulator_ = 0.0f;
        }
    }

    const PhysicsLODSettings& physicsLODSettings() const { return lodSettings_; }

    // --- Phase 2: Fluid Dynamics ---
    SharedPtr<FluidSolver2D> createFluidSolver(LayerID layerId, int w, int h) {
        auto solver = makeShared<FluidSolver2D>(w, h);
        fluidSolvers_[layerId] = solver;
        return solver;
    }

    SharedPtr<FluidSolver2D> getFluidSolver(LayerID layerId) {
        auto it = fluidSolvers_.find(layerId);
        return it != fluidSolvers_.end() ? it->second : nullptr;
    }

    void unregisterFluidSolver(LayerID layerId) {
        fluidSolvers_.erase(layerId);
    }
    
    // --- Phase 3: Soft Body Dynamics ---
    /**
     * @brief レイヤー固有のソフトボディソルバーを登録する
     */
    void registerSoftBody(LayerID layerId, SharedPtr<SoftBodySolver> solver) {
        softBodies_[layerId] = solver;
        softBodySnapshots_.erase(layerId);
    }

    /**
     * @brief レイヤー用ソフトボディソルバーを生成して登録する
     */
    SharedPtr<SoftBodySolver> createSoftBody(LayerID layerId) {
        auto solver = makeShared<SoftBodySolver>();
        softBodies_[layerId] = solver;
        softBodySnapshots_.erase(layerId);
        return solver;
    }

    /**
     * @brief レイヤー用ソフトボディを格子で初期化する
     */
    SharedPtr<SoftBodySolver> createSoftBodyGrid(
        LayerID layerId,
        float left,
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
        auto solver = createSoftBody(layerId);
        solver->buildGrid(left, top, width, height, columns, rows, pointMass, stiffness, pinTopRow, shearStiffness, bendStiffness);
        return solver;
    }

    /**
     * @brief レイヤー用ソフトボディをチェーンで初期化する
     */
    SharedPtr<SoftBodySolver> createSoftBodyChain(
        LayerID layerId,
        float startX,
        float startY,
        float endX,
        float endY,
        int segments,
        float pointMass = 1.0f,
        float stiffness = 1.0f,
        bool pinEnds = false) {
        auto solver = createSoftBody(layerId);
        solver->buildChain(startX, startY, endX, endY, segments, pointMass, stiffness, pinEnds);
        return solver;
    }

    SharedPtr<MpmSolver2D> createMaterialSolver(
        LayerID layerId, MpmMaterialPreset preset = MpmMaterialPreset::Flesh) {
        auto solver = makeShared<MpmSolver2D>();
        solver->applyMaterialPreset(preset);
        materialSolvers_[layerId] = solver;
        materialSnapshots_.erase(layerId);
        return solver;
    }

    SharedPtr<MpmSolver2D> createMaterialGrid(
        LayerID layerId,
        float left, float top, float width, float height,
        int columns = 20, int rows = 20,
        MpmMaterialPreset preset = MpmMaterialPreset::Flesh,
        float density = 1000.0f) {
        const int safeColumns = std::max(2, columns);
        const int safeRows = std::max(2, rows);
        const float cellSize = std::max(
            1.0f,
            std::max(width / static_cast<float>(safeColumns - 1),
                     height / static_cast<float>(safeRows - 1)));
        const int gridWidth = std::max(4, static_cast<int>(std::ceil(width / cellSize)) + 4);
        const int gridHeight = std::max(4, static_cast<int>(std::ceil(height / cellSize)) + 4);

        auto solver = createMaterialSolver(layerId, preset);
        solver->setGrid(cellSize, gridWidth, gridHeight);
        solver->setGridOrigin(left - cellSize * 2.0f, top - cellSize * 2.0f);
        solver->addParticlesGrid(left + width * 0.5f, top + height * 0.5f,
                                 width, height, safeColumns, safeRows, density);
        return solver;
    }

    SharedPtr<MpmSolver2D> getMaterialSolver(LayerID layerId) {
        auto it = materialSolvers_.find(layerId);
        return it != materialSolvers_.end() ? it->second : nullptr;
    }

    void unregisterMaterialSolver(LayerID layerId) {
        materialSolvers_.erase(layerId);
        materialSnapshots_.erase(layerId);
    }

    void registerMaterialCollider(LayerID layerId, const MpmCollider2D& collider) {
        if (const auto solver = getMaterialSolver(layerId)) {
            solver->addCollider(collider);
        }
    }

    void clearMaterialColliders(LayerID layerId) {
        if (const auto solver = getMaterialSolver(layerId)) {
            solver->clearColliders();
        }
    }

    std::vector<MaterialFractureEvent> takeMaterialFractureEvents() {
        auto events = pendingMaterialFractureEvents_.toStdVector();
        pendingMaterialFractureEvents_.clear();
        return events;
    }

    /**
     * @brief レイヤー用 rigid body world を生成して登録する
     */
    SharedPtr<Physics2D> createRigidWorld(LayerID layerId) {
        auto world = makeShared<Physics2D>();
        rigidWorlds_[layerId] = world;
        return world;
    }

    /**
     * @brief レイヤー用 rigid body world を取得する
     */
    SharedPtr<Physics2D> getRigidWorld(LayerID layerId) {
        auto it = rigidWorlds_.find(layerId);
        if (it != rigidWorlds_.end()) return it->second;
        return nullptr;
    }

    /**
     * @brief レイヤー用 rigid body world を解除する
     */
    void unregisterRigidWorld(LayerID layerId) {
        rigidWorlds_.erase(layerId);
    }

    // ---- Cloner/Rigid helpers (thin wrappers, no new simulation state) ----
    std::vector<SharedPtr<RigidBody2D>> createRigidBoxes(
        LayerID layerId, const std::vector<QVector2D>& positions,
        float width, float height,
        float density = 1.0f, float friction = 0.3f, float restitution = 0.2f) {
        auto world = getRigidWorld(layerId);
        if (!world) world = createRigidWorld(layerId);
        std::vector<SharedPtr<RigidBody2D>> out;
        out.reserve(positions.size());
        for (const auto& p : positions) {
            out.push_back(world->addDynamicBox(p.x(), p.y(), width, height, density, friction, restitution));
        }
        return out;
    }

    std::vector<SharedPtr<RigidBody2D>> createRigidCircles(
        LayerID layerId, const std::vector<QVector2D>& positions,
        float radius, float density = 1.0f,
        float friction = 0.3f, float restitution = 0.2f) {
        auto world = getRigidWorld(layerId);
        if (!world) world = createRigidWorld(layerId);
        std::vector<SharedPtr<RigidBody2D>> out;
        out.reserve(positions.size());
        for (const auto& p : positions) {
            out.push_back(world->addDynamicCircle(p.x(), p.y(), radius, density, friction, restitution));
        }
        return out;
    }

    void setSoftBodyWind(LayerID layerId, float dirX, float dirY, float strength) {
        if (auto it = softBodies_.find(layerId); it != softBodies_.end() && it->second) {
            it->second->setWind(dirX, dirY, strength);
        }
    }

    void setSoftBodyTurbulence(LayerID layerId, float strength, float frequency = 1.0f) {
        if (auto it = softBodies_.find(layerId); it != softBodies_.end() && it->second) {
            it->second->setTurbulence(strength, frequency);
        }
    }

    SharedPtr<PyroSimulation> createPyroSimulation(LayerID layerId) {
        auto sim = makeShared<PyroSimulation>();
        pyroSimulations_[layerId] = sim;
        return sim;
    }

    SharedPtr<PyroSimulation> getPyroSimulation(LayerID layerId) {
        auto it = pyroSimulations_.find(layerId);
        return it != pyroSimulations_.end() ? it->second : nullptr;
    }

    void unregisterPyroSimulation(LayerID layerId) {
        pyroSimulations_.erase(layerId);
    }

    void setBoidsConstants(LayerID layerId, const GpuBoidConstants& c) {
        boidsConstants_[layerId] = c;
    }

    std::optional<GpuBoidConstants> getBoidsConstants(LayerID layerId) const {
        auto it = boidsConstants_.find(layerId);
        if (it != boidsConstants_.end()) return it->second;
        return std::nullopt;
    }

    void unregisterBoids(LayerID layerId) {
        boidsConstants_.erase(layerId);
    }

    // ---- Mpm -> ParticleRenderer bridge (manual upload, no auto Composition hook) ----
    ParticleRenderData buildMpmParticleRenderData(
        LayerID layerId, float particleSize = 3.0f, float alpha = 1.0f) const {
        ParticleRenderData out;
        auto it = materialSolvers_.find(layerId);
        if (it == materialSolvers_.end() || !it->second) return out;
        const auto& particles = it->second->particles();
        out.particles.reserve(particles.size());
        for (const auto& p : particles) {
            if (!p.active) continue;
            ParticleVertex v;
            v.px = p.pos.x; v.py = p.pos.y; v.pz = 0.0f;
            v.vx = p.vel.x; v.vy = p.vel.y; v.vz = 0.0f;
            v.r = p.r; v.g = p.g; v.b = p.b; v.a = alpha;
            v.size = particleSize;
            v.age = 0.0f; v.lifetime = 1.0f;
            out.particles.push_back(v);
        }
        return out;
    }

    /**
     * @brief レイヤー用ソフトボディソルバーを解除する
     */
    void unregisterSoftBody(LayerID layerId) {
        softBodies_.erase(layerId);
        softBodyColliders_.erase(layerId);
        softBodySnapshots_.erase(layerId);
    }

    /**
     * @brief レイヤー固有のソフトボディ collider を登録する
     */
    void registerSoftBodyCollider(LayerID layerId, const SoftBodyCollider& collider) {
        softBodyColliders_[layerId].push_back(collider);
    }

    /**
     * @brief レイヤー固有のソフトボディ collider を全て消す
     */
    void clearSoftBodyColliders(LayerID layerId) {
        softBodyColliders_.erase(layerId);
    }
    
    /**
     * @brief 指定したレイヤーのソフトボディソルバーを取得する
     */
    SharedPtr<SoftBodySolver> getSoftBody(LayerID layerId) {
        auto it = softBodies_.find(layerId);
        if (it != softBodies_.end()) return it->second;
        return nullptr;
    }

    /**
     * @brief レイヤー用 collider 一覧を取得する
     */
    std::vector<SoftBodyCollider> getSoftBodyColliders(LayerID layerId) const {
        auto it = softBodyColliders_.find(layerId);
        if (it != softBodyColliders_.end()) {
            return it->second.toStdVector();
        }
        return {};
    }

    void captureSoftBodySnapshots(int64_t frame) {
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            auto& snapshots = softBodySnapshots_[layerId];
            snapshots[frame] = solver->snapshot();
            while (snapshots.size() > maxSoftBodySnapshotsPerLayer_) {
                snapshots.erase(snapshots.begin());
            }
        }
        for (const auto& [layerId, solver] : materialSolvers_) {
            if (!solver) continue;
            auto& snapshots = materialSnapshots_[layerId];
            snapshots[frame] = solver->snapshot();
            while (snapshots.size() > maxMaterialSnapshotsPerLayer_) {
                snapshots.erase(snapshots.begin());
            }
        }
    }

    bool restoreSoftBodySnapshots(int64_t frame) {
        // Validate every target first so a cache miss never restores only a
        // subset of layers in a composition.
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            const auto cacheIt = softBodySnapshots_.find(layerId);
            if (cacheIt == softBodySnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !solver->canRestoreSnapshot(snapshotIt->second)) {
                return false;
            }
        }
        for (const auto& [layerId, solver] : materialSolvers_) {
            if (!solver) continue;
            const auto cacheIt = materialSnapshots_.find(layerId);
            if (cacheIt == materialSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !solver->canRestoreSnapshot(snapshotIt->second)) {
                return false;
            }
        }
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            solver->restoreSnapshot(softBodySnapshots_.at(layerId).at(frame));
        }
        for (const auto& [layerId, solver] : materialSolvers_) {
            if (!solver) continue;
            solver->restoreSnapshot(materialSnapshots_.at(layerId).at(frame));
        }
        return true;
    }

    /**
     * @brief 全ての物理シミュレーションを更新する
     * @param dt 経過時間（秒）
     * @param gravity 重力加速度（デフォルト 9.8 [m/s^2]）
     */
    void update(float dt, float gravityX = 0.0f, float gravityY = 9.8f) {
        if (dt <= 0.0f) return;

        float simulationDt = dt;
        if (lodSettings_.level == PhysicsLODLevel::Frozen) return;
        if (lodSettings_.targetHz > 0.0f) {
            lodAccumulator_ += dt;
            const float interval = 1.0f / lodSettings_.targetHz;
            if (lodAccumulator_ < interval) return;
            simulationDt = std::min(lodAccumulator_, interval * 4.0f);
            lodAccumulator_ = 0.0f;
        }

        for (auto& [id, fs] : fluidSolvers_) {
            if (fs) {
                if (lodSettings_.fluidSolverIterations > 0) fs->setSolverIterations(lodSettings_.fluidSolverIterations);
                fs->update(simulationDt);
            }
        }
        
        for (auto& [id, sb] : softBodies_) {
            // ソフトボディは Verlet 積分と拘束解決で更新
            auto colliderIt = softBodyColliders_.find(id);
            if (colliderIt != softBodyColliders_.end()) {
                sb->clearColliders();
                for (const auto& collider : colliderIt->second) {
                    sb->addCollider(collider);
                }
            }
            if (lodSettings_.softBodyMaxSubSteps > 0) {
                sb->setMaxSubsteps(lodSettings_.softBodyMaxSubSteps);
            }
            if (lodSettings_.softBodyConstraintIterations > 0) {
                sb->setConstraintIterations(lodSettings_.softBodyConstraintIterations);
            }
            if (lodSettings_.softBodyCollisionIterations > 0) {
                sb->setCollisionIterations(lodSettings_.softBodyCollisionIterations);
            }
            if (lodSettings_.softBodyGridScale < 0.999f) {
                sb->reduceGridResolution(lodSettings_.softBodyGridScale);
            } else {
                sb->restoreGridResolution();
            }
            if (lodSettings_.disableSoftBodySelfCollision) {
                sb->setSelfCollisionEnabled(false);
            }
            sb->update(simulationDt, gravityX, gravityY);
        }

        for (auto& [id, solver] : materialSolvers_) {
            if (solver) {
                solver->setFractureEnabled(!lodSettings_.disableFracture);
                if (lodSettings_.materialMaxSubSteps > 0) {
                    solver->setMaxSubsteps(lodSettings_.materialMaxSubSteps);
                }
                solver->update(simulationDt);
                const int fracturedCount = solver->fractureEventCount();
                if (fracturedCount > 0) {
                    pendingMaterialFractureEvents_.push_back(
                        {id, fracturedCount, solver->particleCount()});
                    solver->clearFractureEvents();
                }
            }
        }

        for (auto& [id, world] : rigidWorlds_) {
            if (world) {
                if (lodSettings_.applySleepPolicy || lodSettings_.disableContinuousCollision) {
                    for (const auto& body : world->getBodies()) {
                        if (!body) continue;
                        if (lodSettings_.applySleepPolicy) {
                            body->enableSleep(true);
                            body->setSleepThreshold(lodSettings_.sleepThreshold);
                        }
                        if (lodSettings_.disableContinuousCollision) {
                            body->setContinuousCollision(false);
                        }
                        if (lodSettings_.simplifyCollisionMesh) {
                            body->simplifyCollisionMesh();
                        } else {
                            body->restoreCollisionMesh();
                        }
                    }
                }
                world->step(simulationDt, lodSettings_.rigidBodySubSteps > 0
                    ? lodSettings_.rigidBodySubSteps : 4);
            }
        }

        for (auto& [id, pyro] : pyroSimulations_) {
            if (pyro) pyro->step(simulationDt);
        }
    }

    /**
     * @brief シミュレーションを全て破棄する
     */
    void clear() {
        fluidSolvers_.clear();
        softBodies_.clear();
        softBodyColliders_.clear();
        softBodySnapshots_.clear();
        materialSolvers_.clear();
        materialSnapshots_.clear();
        pendingMaterialFractureEvents_.clear();
        rigidWorlds_.clear();
        pyroSimulations_.clear();
        boidsConstants_.clear();
    }

private:
    PhysicsSystem() = default;
    ~PhysicsSystem() = default;

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    
    std::map<LayerID, SharedPtr<FluidSolver2D>> fluidSolvers_;
    std::map<LayerID, SharedPtr<SoftBodySolver>> softBodies_;
    std::map<LayerID, NamedVector<SoftBodyCollider>> softBodyColliders_;
    std::map<LayerID, std::map<int64_t, SoftBodySnapshot>> softBodySnapshots_;
    std::map<LayerID, SharedPtr<MpmSolver2D>> materialSolvers_;
    std::map<LayerID, std::map<int64_t, MpmSnapshot2D>> materialSnapshots_;
    NamedVector<MaterialFractureEvent> pendingMaterialFractureEvents_;
    std::map<LayerID, SharedPtr<Physics2D>> rigidWorlds_;
    std::map<LayerID, SharedPtr<PyroSimulation>> pyroSimulations_;
    std::map<LayerID, GpuBoidConstants> boidsConstants_;
    PhysicsLODSettings lodSettings_;
    float lodAccumulator_ = 0.0f;
    static constexpr std::size_t maxSoftBodySnapshotsPerLayer_ = 480;
    static constexpr std::size_t maxMaterialSnapshotsPerLayer_ = 480;
};

} // namespace ArtifactCore
