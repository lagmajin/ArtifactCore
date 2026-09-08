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
import Physics.Cloth3D;
import Physics.Mpm2D;
#ifdef ARTIFACT_ENABLE_PYRO
import Core.Simulation.Pyro;
#endif
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

export struct PhysicsTimelineSettings {
    float fixedTimeStep = 1.0f / 60.0f;
    int maxSubsteps = 8;
    std::size_t maxCachedFrames = 480;
};

// Renderer-facing snapshot of a Cloth3D grid.  Keep the solver type and its
// implementation-only module behind PhysicsSystem so high-level consumers do
// not import Physics.Cloth3D merely to read current deformation.
export struct ClothDeformationMesh3D {
    std::vector<float> positions;
    std::vector<float> uvs;
    std::vector<std::uint32_t> indices;

    bool isValid() const noexcept {
        return !positions.empty() && !indices.empty() &&
               positions.size() % 3 == 0 &&
               uvs.size() * 3 == positions.size() * 2;
    }
};

/**
 * @brief 物理演算システム。コンポジション内のシミュレーションを統合管理する。
 * UIを持たない「Core」レイヤーでのシミュレーション実行を担う。
 */
export class PhysicsSystem {
public:
    static PhysicsSystem& instance() {
        // Compositions may outlive other function-local service singletons
        // during CRT teardown and unregister their worlds from here.  Keep this
        // process-lifetime service valid until process termination instead of
        // relying on cross-singleton static destruction order.
        static PhysicsSystem* const inst = new PhysicsSystem();
        return *inst;
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

    void setPhysicsTimelineSettings(const PhysicsTimelineSettings& settings) {
        timelineSettings_.fixedTimeStep = std::clamp(
            settings.fixedTimeStep, 1.0f / 1000.0f, 1.0f / 15.0f);
        timelineSettings_.maxSubsteps = std::clamp(settings.maxSubsteps, 1, 64);
        timelineSettings_.maxCachedFrames = std::max<std::size_t>(
            1, settings.maxCachedFrames);
        timelineAccumulator_ = 0.0f;
        invalidatePhysicsSnapshots();
        trimPhysicsSnapshots();
    }

    const PhysicsTimelineSettings& physicsTimelineSettings() const {
        return timelineSettings_;
    }

    void invalidatePhysicsSnapshots() {
        fluidSnapshots_.clear();
        softBodySnapshots_.clear();
        cloth3DSnapshots_.clear();
        materialSnapshots_.clear();
        rigidSnapshots_.clear();
        compositionRigidSnapshots_.clear();
    }

    // Advance every registered solver through the same fixed simulation clock.
    // Solver-specific internal substeps remain owned by the solver; this
    // method only owns frame-time accumulation and the outer step boundary.
    void advancePhysicsFrame(float frameDeltaSeconds,
                             float gravityX = 0.0f,
                             float gravityY = 9.8f,
                             bool includeCompositionRigidWorlds = true) {
        if (frameDeltaSeconds <= 0.0f ||
            lodSettings_.level == PhysicsLODLevel::Frozen) {
            return;
        }

        const float fixedDt = timelineSettings_.fixedTimeStep;
        const float maxAccumulated = fixedDt *
            static_cast<float>(timelineSettings_.maxSubsteps);
        timelineAccumulator_ = std::min(
            timelineAccumulator_ + frameDeltaSeconds, maxAccumulated);

        fixedTimelineMode_ = true;
        int steps = 0;
        while (timelineAccumulator_ + 1.0e-7f >= fixedDt &&
               steps < timelineSettings_.maxSubsteps) {
            update(fixedDt, gravityX, gravityY, includeCompositionRigidWorlds);
            timelineAccumulator_ -= fixedDt;
            ++steps;
        }
        fixedTimelineMode_ = false;
    }

    // --- Phase 2: Fluid Dynamics ---
    SharedPtr<FluidSolver2D> createFluidSolver(LayerID layerId, int w, int h) {
        auto solver = makeShared<FluidSolver2D>(w, h);
        fluidSolvers_[layerId] = solver;
        invalidatePhysicsSnapshots();
        return solver;
    }

    SharedPtr<FluidSolver2D> getFluidSolver(LayerID layerId) {
        auto it = fluidSolvers_.find(layerId);
        return it != fluidSolvers_.end() ? it->second : nullptr;
    }

    void unregisterFluidSolver(LayerID layerId) {
        fluidSolvers_.erase(layerId);
        invalidatePhysicsSnapshots();
    }
    
    // --- Phase 3: Soft Body Dynamics ---
    /**
     * @brief レイヤー固有のソフトボディソルバーを登録する
     */
    void registerSoftBody(LayerID layerId, SharedPtr<SoftBodySolver> solver) {
        softBodies_[layerId] = solver;
        invalidatePhysicsSnapshots();
    }

    /**
     * @brief レイヤー用ソフトボディソルバーを生成して登録する
     */
    SharedPtr<SoftBodySolver> createSoftBody(LayerID layerId) {
        auto solver = makeShared<SoftBodySolver>();
        softBodies_[layerId] = solver;
        invalidatePhysicsSnapshots();
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
        setMaterialSolver(layerId, solver);
        invalidatePhysicsSnapshots();
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
        if (const auto* solver = findMaterialSolver(layerId)) {
            return *solver;
        }
        return nullptr;
    }

    void unregisterMaterialSolver(LayerID layerId) {
        removeMaterialSolver(layerId);
        invalidatePhysicsSnapshots();
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
        invalidatePhysicsSnapshots();
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
        invalidatePhysicsSnapshots();
    }

    SharedPtr<Physics2D> createCompositionRigidWorld(CompositionID compositionId) {
        auto world = makeShared<Physics2D>();
        compositionRigidWorlds_[compositionId] = world;
        invalidatePhysicsSnapshots();
        return world;
    }

    SharedPtr<Physics2D> getCompositionRigidWorld(CompositionID compositionId) {
        auto it = compositionRigidWorlds_.find(compositionId);
        return it != compositionRigidWorlds_.end() ? it->second : nullptr;
    }

    void unregisterCompositionRigidWorld(CompositionID compositionId) {
        compositionRigidWorlds_.erase(compositionId);
        invalidatePhysicsSnapshots();
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

#ifdef ARTIFACT_ENABLE_PYRO
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
#endif

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
        const auto* solver = findMaterialSolver(layerId);
        if (!solver || !*solver) return out;
        const auto& particles = (*solver)->particles();
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
        invalidatePhysicsSnapshots();
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

    // --- Cloth3D: SoftBody2Dと状態を共有しない独立レジストリ ---
    SharedPtr<ClothSolver3D> createCloth3D(LayerID layerId) {
        auto solver = makeShared<ClothSolver3D>();
        cloth3DBodies_[layerId] = solver;
        invalidatePhysicsSnapshots();
        return solver;
    }

    SharedPtr<ClothSolver3D> createCloth3DGrid(
        LayerID layerId,
        float left,
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
        auto solver = createCloth3D(layerId);
        solver->buildGrid(left, top, width, height, depth, columns, rows,
                          pointMass, stiffness, pinTopRow, shearStiffness);
        return solver;
    }

    SharedPtr<ClothSolver3D> getCloth3D(LayerID layerId) {
        auto it = cloth3DBodies_.find(layerId);
        if (it != cloth3DBodies_.end()) return it->second;
        return nullptr;
    }

    bool hasCloth3D(LayerID layerId) const {
        const auto it = cloth3DBodies_.find(layerId);
        return it != cloth3DBodies_.end() && static_cast<bool>(it->second);
    }

    ClothDeformationMesh3D cloth3DDeformationMesh(LayerID layerId) const {
        ClothDeformationMesh3D mesh;
        const auto it = cloth3DBodies_.find(layerId);
        if (it == cloth3DBodies_.end() || !it->second) {
            return mesh;
        }

        const auto& solver = *it->second;
        const int columns = solver.gridColumns();
        const int rows = solver.gridRows();
        if (columns < 2 || rows < 2 ||
            solver.pointCount() != static_cast<std::size_t>(columns * rows)) {
            return mesh;
        }

        mesh.positions.reserve(solver.pointCount() * 3);
        mesh.uvs.reserve(solver.pointCount() * 2);
        const float invColumns = 1.0f / static_cast<float>(columns - 1);
        const float invRows = 1.0f / static_cast<float>(rows - 1);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                const auto& point = solver.point(
                    static_cast<std::size_t>(y * columns + x));
                mesh.positions.push_back(point.x);
                mesh.positions.push_back(point.y);
                mesh.positions.push_back(point.z);
                mesh.uvs.push_back(static_cast<float>(x) * invColumns);
                mesh.uvs.push_back(static_cast<float>(y) * invRows);
            }
        }
        mesh.indices = solver.getGridTriangleIndices();
        return mesh;
    }

    void unregisterCloth3D(LayerID layerId) {
        cloth3DBodies_.erase(layerId);
        invalidatePhysicsSnapshots();
    }

    void setCloth3DWind(LayerID layerId, float dirX, float dirY, float dirZ, float strength) {
        if (auto it = cloth3DBodies_.find(layerId); it != cloth3DBodies_.end() && it->second) {
            it->second->setWind(dirX, dirY, dirZ, strength);
        }
    }

    // Capture all registered solver states at one logical frame. Keeping the
    // frame key common is what lets seek/loop restore a mixed rigid + soft +
    // fluid composition without partially rewinding it.
    void capturePhysicsSnapshots(int64_t frame) {
        for (const auto& [layerId, solver] : fluidSolvers_) {
            if (!solver) continue;
            fluidSnapshots_[layerId][frame] = solver->snapshot();
        }
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            auto& snapshots = softBodySnapshots_[layerId];
            snapshots[frame] = solver->snapshot();
        }
        for (const auto& [layerId, solver] : cloth3DBodies_) {
            if (!solver) continue;
            auto& snapshots = cloth3DSnapshots_[layerId];
            snapshots[frame] = solver->snapshot();
        }
        for (const auto& entry : materialSolvers_) {
            const auto& layerId = entry.layerId;
            const auto& solver = entry.solver;
            if (!solver) continue;
            auto& snapshots = materialSnapshots_[layerId];
            snapshots[frame] = makeShared<MpmSnapshot2D>(solver->snapshot());
        }
        for (const auto& [layerId, world] : rigidWorlds_) {
            if (!world) continue;
            rigidSnapshots_[layerId][frame] = world->snapshot();
        }
        for (const auto& [compositionId, world] : compositionRigidWorlds_) {
            if (!world) continue;
            compositionRigidSnapshots_[compositionId][frame] = world->snapshot();
        }
        trimPhysicsSnapshots();
    }

    // Backward-compatible entry point. Existing callers that only know the
    // old name now participate in the same mixed-solver cache.
    void captureSoftBodySnapshots(int64_t frame) {
        capturePhysicsSnapshots(frame);
    }

    bool restorePhysicsSnapshots(int64_t frame) {
        // Validate every target first so a cache miss never restores only a
        // subset of layers in a composition.
        for (const auto& [layerId, solver] : fluidSolvers_) {
            if (!solver) continue;
            const auto cacheIt = fluidSnapshots_.find(layerId);
            if (cacheIt == fluidSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !solver->canRestoreSnapshot(snapshotIt->second)) {
                return false;
            }
        }
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
        for (const auto& [layerId, solver] : cloth3DBodies_) {
            if (!solver) continue;
            const auto cacheIt = cloth3DSnapshots_.find(layerId);
            if (cacheIt == cloth3DSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !solver->canRestoreSnapshot(snapshotIt->second)) {
                return false;
            }
        }
        for (const auto& entry : materialSolvers_) {
            const auto& layerId = entry.layerId;
            const auto& solver = entry.solver;
            if (!solver) continue;
            const auto cacheIt = materialSnapshots_.find(layerId);
            if (cacheIt == materialSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !snapshotIt->second ||
                !solver->canRestoreSnapshot(*snapshotIt->second)) {
                return false;
            }
        }
        for (const auto& [layerId, world] : rigidWorlds_) {
            if (!world) continue;
            const auto cacheIt = rigidSnapshots_.find(layerId);
            if (cacheIt == rigidSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !world->canRestoreSnapshot(snapshotIt->second)) return false;
        }
        for (const auto& [compositionId, world] : compositionRigidWorlds_) {
            if (!world) continue;
            const auto cacheIt = compositionRigidSnapshots_.find(compositionId);
            if (cacheIt == compositionRigidSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !world->canRestoreSnapshot(snapshotIt->second)) return false;
        }
        for (const auto& [layerId, world] : rigidWorlds_) {
            if (!world) continue;
            if (!world->restoreSnapshot(rigidSnapshots_.at(layerId).at(frame))) {
                return false;
            }
        }
        for (const auto& [compositionId, world] : compositionRigidWorlds_) {
            if (!world) continue;
            if (!world->restoreSnapshot(
                    compositionRigidSnapshots_.at(compositionId).at(frame))) {
                return false;
            }
        }
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            solver->restoreSnapshot(softBodySnapshots_.at(layerId).at(frame));
        }
        for (const auto& [layerId, solver] : cloth3DBodies_) {
            if (!solver) continue;
            solver->restoreSnapshot(cloth3DSnapshots_.at(layerId).at(frame));
        }
        for (const auto& [layerId, solver] : fluidSolvers_) {
            if (!solver) continue;
            solver->restoreSnapshot(fluidSnapshots_.at(layerId).at(frame));
        }
        for (const auto& entry : materialSolvers_) {
            const auto& layerId = entry.layerId;
            const auto& solver = entry.solver;
            if (!solver) continue;
            solver->restoreSnapshot(*materialSnapshots_.at(layerId).at(frame));
        }
        return true;
    }

    bool restoreSoftBodySnapshots(int64_t frame) {
        // Preserve the legacy partial-restore contract for existing layer
        // callers. Mixed-solver all-or-nothing restoration is exposed through
        // restorePhysicsSnapshots() and can be adopted by composition seek
        // once the rigid/fluid timeline owner is wired there.
        // Cloth3D shares this legacy entry so existing composition seek
        // keeps working without migrating every caller immediately.
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
        for (const auto& [layerId, solver] : cloth3DBodies_) {
            if (!solver) continue;
            const auto cacheIt = cloth3DSnapshots_.find(layerId);
            if (cacheIt == cloth3DSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !solver->canRestoreSnapshot(snapshotIt->second)) {
                return false;
            }
        }
        for (const auto& entry : materialSolvers_) {
            const auto& layerId = entry.layerId;
            const auto& solver = entry.solver;
            if (!solver) continue;
            const auto cacheIt = materialSnapshots_.find(layerId);
            if (cacheIt == materialSnapshots_.end()) return false;
            const auto snapshotIt = cacheIt->second.find(frame);
            if (snapshotIt == cacheIt->second.end() ||
                !snapshotIt->second ||
                !solver->canRestoreSnapshot(*snapshotIt->second)) {
                return false;
            }
        }
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            solver->restoreSnapshot(softBodySnapshots_.at(layerId).at(frame));
        }
        for (const auto& [layerId, solver] : cloth3DBodies_) {
            if (!solver) continue;
            solver->restoreSnapshot(cloth3DSnapshots_.at(layerId).at(frame));
        }
        for (const auto& entry : materialSolvers_) {
            const auto& layerId = entry.layerId;
            const auto& solver = entry.solver;
            if (!solver) continue;
            solver->restoreSnapshot(*materialSnapshots_.at(layerId).at(frame));
        }
        return true;
    }

    /**
     * @brief 全ての物理シミュレーションを更新する
     * @param dt 経過時間（秒）
     * @param gravity 重力加速度（デフォルト 9.8 [m/s^2]）
     */
    // Composition playback owns its shared world's fixed clock. Never step
    // other compositions as a side effect of advancing this one.
    void updateCompositionRigidWorld(CompositionID id, float dt) {
        if (dt <= 0.0f || lodSettings_.level == PhysicsLODLevel::Frozen) return;
        if (auto world = getCompositionRigidWorld(id)) {
            // PERF: 空 world の step は何も変えない (body 無し → contact 不可 →
            // contactEvents は空のまま)。plain 構成で最大 8 catch-up × 4 substep
            // の b2World_Step を省略する。
            if (!world->hasBodies()) return;
            world->step(dt, lodSettings_.rigidBodySubSteps > 0
                ? lodSettings_.rigidBodySubSteps : 4);
        }
    }

    void update(float dt, float gravityX = 0.0f, float gravityY = 9.8f,
                bool includeCompositionRigidWorlds = true) {
        if (dt <= 0.0f) return;

        float simulationDt = dt;
        if (lodSettings_.level == PhysicsLODLevel::Frozen) return;
        if (lodSettings_.targetHz > 0.0f && !fixedTimelineMode_) {
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

        for (auto& [id, cloth] : cloth3DBodies_) {
            if (!cloth) continue;
            if (lodSettings_.softBodyMaxSubSteps > 0) {
                cloth->setMaxSubsteps(lodSettings_.softBodyMaxSubSteps);
            }
            if (lodSettings_.softBodyConstraintIterations > 0) {
                cloth->setConstraintIterations(lodSettings_.softBodyConstraintIterations);
            }
            if (lodSettings_.softBodyCollisionIterations > 0) {
                cloth->setCollisionIterations(lodSettings_.softBodyCollisionIterations);
            }
            cloth->update(simulationDt, gravityX, gravityY, 0.0f);
        }

        for (auto& entry : materialSolvers_) {
            const auto& id = entry.layerId;
            auto& solver = entry.solver;
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

        for (auto& [id, world] : compositionRigidWorlds_) {
            if (includeCompositionRigidWorlds && world) {
                world->step(simulationDt, lodSettings_.rigidBodySubSteps > 0
                    ? lodSettings_.rigidBodySubSteps : 4);
            }
        }

#ifdef ARTIFACT_ENABLE_PYRO
        for (auto& [id, pyro] : pyroSimulations_) {
            if (pyro) pyro->step(simulationDt);
        }
#endif
    }

    /**
     * @brief シミュレーションを全て破棄する
     */
    void clear() {
        fluidSolvers_.clear();
        softBodies_.clear();
        softBodyColliders_.clear();
        cloth3DBodies_.clear();
        materialSolvers_.clear();
        pendingMaterialFractureEvents_.clear();
        rigidWorlds_.clear();
        compositionRigidWorlds_.clear();
        invalidatePhysicsSnapshots();
        timelineAccumulator_ = 0.0f;
#ifdef ARTIFACT_ENABLE_PYRO
        pyroSimulations_.clear();
#endif
        boidsConstants_.clear();
    }

private:
    void trimPhysicsSnapshots() {
        const auto trim = [limit = timelineSettings_.maxCachedFrames](auto& cache) {
            for (auto& entry : cache) {
                auto& snapshots = entry.second;
                while (snapshots.size() > limit) {
                    snapshots.erase(snapshots.begin());
                }
            }
        };
        trim(fluidSnapshots_);
        trim(softBodySnapshots_);
        trim(cloth3DSnapshots_);
        trim(materialSnapshots_);
        trim(rigidSnapshots_);
        trim(compositionRigidSnapshots_);
    }

    struct MaterialSolverEntry {
        LayerID layerId;
        SharedPtr<MpmSolver2D> solver;
    };

    SharedPtr<MpmSolver2D>* findMaterialSolver(LayerID layerId) {
        for (auto& entry : materialSolvers_) {
            if (entry.layerId == layerId) return &entry.solver;
        }
        return nullptr;
    }

    const SharedPtr<MpmSolver2D>* findMaterialSolver(LayerID layerId) const {
        for (const auto& entry : materialSolvers_) {
            if (entry.layerId == layerId) return &entry.solver;
        }
        return nullptr;
    }

    void setMaterialSolver(LayerID layerId, SharedPtr<MpmSolver2D> solver) {
        if (auto* existing = findMaterialSolver(layerId)) {
            *existing = std::move(solver);
            return;
        }
        materialSolvers_.add(MaterialSolverEntry{layerId, std::move(solver)});
    }

    bool removeMaterialSolver(LayerID layerId) {
        return materialSolvers_.removeIf(
            [layerId](const MaterialSolverEntry& entry) {
                return entry.layerId == layerId;
            }) != 0;
    }

    PhysicsSystem() = default;
    ~PhysicsSystem();

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    
    std::map<LayerID, SharedPtr<FluidSolver2D>> fluidSolvers_;
    std::map<LayerID, std::map<int64_t, FluidSnapshot2D>> fluidSnapshots_;
    std::map<LayerID, SharedPtr<SoftBodySolver>> softBodies_;
    std::map<LayerID, NamedVector<SoftBodyCollider>> softBodyColliders_;
    std::map<LayerID, std::map<int64_t, SoftBodySnapshot>> softBodySnapshots_;
    std::map<LayerID, SharedPtr<ClothSolver3D>> cloth3DBodies_;
    std::map<LayerID, std::map<int64_t, ClothSnapshot3D>> cloth3DSnapshots_;
    NamedVector<MaterialSolverEntry> materialSolvers_;
    std::map<LayerID, std::map<int64_t, SharedPtr<MpmSnapshot2D>>> materialSnapshots_;
    NamedVector<MaterialFractureEvent> pendingMaterialFractureEvents_;
    std::map<LayerID, SharedPtr<Physics2D>> rigidWorlds_;
    std::map<LayerID, std::map<int64_t, Physics2DSnapshot>> rigidSnapshots_;
    std::map<CompositionID, SharedPtr<Physics2D>> compositionRigidWorlds_;
    std::map<CompositionID, std::map<int64_t, Physics2DSnapshot>> compositionRigidSnapshots_;
#ifdef ARTIFACT_ENABLE_PYRO
    std::map<LayerID, SharedPtr<PyroSimulation>> pyroSimulations_;
#endif
    std::map<LayerID, GpuBoidConstants> boidsConstants_;
    PhysicsLODSettings lodSettings_;
    PhysicsTimelineSettings timelineSettings_;
    float lodAccumulator_ = 0.0f;
    float timelineAccumulator_ = 0.0f;
    bool fixedTimelineMode_ = false;
};

// Keep destruction of container-held solver ownership out of the exported
// class definition so the module interface does not eagerly instantiate it.
PhysicsSystem::~PhysicsSystem() = default;

} // namespace ArtifactCore
