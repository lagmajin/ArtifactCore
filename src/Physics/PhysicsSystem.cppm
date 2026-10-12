module;
#include <utility>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
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

template <typename K, typename V>
class PhysicsOrderedRegistry {
public:
    struct Entry {
        K first;
        V second;
    };

    auto begin() noexcept { return entries_.begin(); }
    auto end() noexcept { return entries_.end(); }
    auto begin() const noexcept { return entries_.begin(); }
    auto end() const noexcept { return entries_.end(); }

    auto find(const K& key) noexcept {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (sameKey(it->first, key)) return it;
        }
        return entries_.end();
    }

    auto find(const K& key) const noexcept {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (sameKey(it->first, key)) return it;
        }
        return entries_.end();
    }

    V& operator[](const K& key) {
        std::size_t insertionIndex = entries_.size();
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (sameKey(entries_[i].first, key)) return entries_[i].second;
            if (key < entries_[i].first) {
                insertionIndex = i;
                break;
            }
        }
        entries_.insert(insertionIndex, Entry{key, V{}});
        return entries_[insertionIndex].second;
    }

    V& at(const K& key) { return find(key)->second; }
    const V& at(const K& key) const { return find(key)->second; }

    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }
    void reserve(std::size_t capacity) { entries_.reserve(capacity); }
    void clear() noexcept { entries_.clear(); }

    std::size_t erase(const K& key) {
        const auto it = find(key);
        if (it == entries_.end()) return 0;
        entries_.removeAt(static_cast<std::size_t>(it - entries_.begin()));
        return 1;
    }

    template <typename Iterator>
    void erase(Iterator it) {
        if (it == entries_.end()) return;
        entries_.removeAt(static_cast<std::size_t>(it - entries_.begin()));
    }

private:
    static bool sameKey(const K& left, const K& right) {
        return !(left < right) && !(right < left);
    }

    NamedVector<Entry> entries_{ContainerName{"Physics.OrderedRegistry"}};
};

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
    NamedVector<float> positions{ContainerName{"Physics.ClothPositions"}};
    NamedVector<float> uvs{ContainerName{"Physics.ClothUvs"}};
    NamedVector<std::uint32_t> indices{ContainerName{"Physics.ClothIndices"}};

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
    static constexpr int64_t kGranularMaterialSnapshotInterval = 12;
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
        const float requestedFixedTimeStep =
            std::isfinite(settings.fixedTimeStep)
                ? settings.fixedTimeStep
                : 1.0f / 60.0f;
        timelineSettings_.fixedTimeStep = std::clamp(
            requestedFixedTimeStep, 1.0f / 1000.0f, 1.0f / 15.0f);
        timelineSettings_.maxSubsteps = std::clamp(settings.maxSubsteps, 1, 64);
        timelineSettings_.maxCachedFrames = std::max<std::size_t>(
            1, settings.maxCachedFrames);
        timelineAccumulator_ = 0.0f;
        // The granular solvers own their CFL timestep, so a global physics
        // timeline setting change only invalidates frame checkpoints. Keep
        // the live granular clocks and rebuild seek history from current
        // solver state on the next replay.
        for (auto& [simulationWorldId, worldSnapshots] :
             granularMaterialSnapshots_) {
            worldSnapshots.clear();
            for (const auto& entry : granularMaterialSolvers_) {
                if (entry.simulationWorldId == simulationWorldId) {
                    worldSnapshots[entry.layerId];
                }
            }
        }
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
        granularMaterialSnapshots_.clear();
    }

    // Advance every registered solver through the same fixed simulation clock.
    // Solver-specific internal substeps remain owned by the solver; this
    // method only owns frame-time accumulation and the outer step boundary.
    void advancePhysicsFrame(float frameDeltaSeconds,
                             float gravityX = 0.0f,
                             float gravityY = 9.8f,
                             bool includeCompositionRigidWorlds = true,
                             std::uint64_t simulationWorldId = 0) {
        if (!std::isfinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0f ||
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
        advanceGranularMaterialSolvers(frameDeltaSeconds, simulationWorldId);
    }

    bool granularMaterialReplayRequired(
        std::uint64_t simulationWorldId) const {
        for (const auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                return clock.requiresReplay;
            }
        }
        return false;
    }

    bool hasGranularMaterialSolvers(
        std::uint64_t simulationWorldId) const {
        if (simulationWorldId == 0) return false;
        return std::any_of(
            granularMaterialSolvers_.begin(), granularMaterialSolvers_.end(),
            [simulationWorldId](const GranularMaterialSolverEntry& entry) {
                return entry.simulationWorldId == simulationWorldId &&
                       static_cast<bool>(entry.solver);
            });
    }

    float granularMaterialBacklogSeconds(
        std::uint64_t simulationWorldId, LayerID layerId) const {
        if (simulationWorldId == 0) return 0.0f;
        float solverBacklog = 0.0f;
        bool foundSolver = false;
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId ||
                entry.layerId != layerId || !entry.solver) {
                continue;
            }
            solverBacklog = entry.solver->accumulatedTime();
            foundSolver = true;
            break;
        }
        if (!foundSolver) return 0.0f;
        float worldBacklog = 0.0f;
        for (const auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                worldBacklog = clock.accumulator;
                break;
            }
        }
        const float total = solverBacklog + worldBacklog;
        return std::isfinite(total) ? std::max(0.0f, total) : 0.0f;
    }

    void completeGranularMaterialReplay(
        std::uint64_t simulationWorldId) {
        for (auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                clock.requiresReplay = false;
                return;
            }
        }
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
        if (!std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(width) || !std::isfinite(height) ||
            !std::isfinite(density) || width <= 0.0f || height <= 0.0f ||
            density <= 0.0f) {
            return nullptr;
        }
        const int safeColumns = std::max(2, columns);
        const int safeRows = std::max(2, rows);
        const float cellSize = std::max(
            1.0f,
            std::max(width / static_cast<float>(safeColumns - 1),
                     height / static_cast<float>(safeRows - 1)));
        const double gridWidthValue =
            std::ceil(static_cast<double>(width) / cellSize) + 4.0;
        const double gridHeightValue =
            std::ceil(static_cast<double>(height) / cellSize) + 4.0;
        if (!std::isfinite(gridWidthValue) ||
            !std::isfinite(gridHeightValue) ||
            gridWidthValue > std::numeric_limits<int>::max() ||
            gridHeightValue > std::numeric_limits<int>::max()) {
            return nullptr;
        }
        const int gridWidth = std::max(4, static_cast<int>(gridWidthValue));
        const int gridHeight = std::max(4, static_cast<int>(gridHeightValue));

        auto solver = makeShared<MpmSolver2D>();
        solver->applyMaterialPreset(preset);
        if (!solver->setGrid(cellSize, gridWidth, gridHeight) ||
            !solver->setGridOrigin(left - cellSize * 2.0f,
                                   top - cellSize * 2.0f)) {
            return nullptr;
        }
        solver->addParticlesGrid(left + width * 0.5f, top + height * 0.5f,
                                 width, height, safeColumns, safeRows, density);
        const std::size_t expectedParticleCount =
            static_cast<std::size_t>(safeColumns) *
            static_cast<std::size_t>(safeRows);
        if (solver->particles().size() != expectedParticleCount) {
            return nullptr;
        }
        setMaterialSolver(layerId, solver);
        invalidatePhysicsSnapshots();
        return solver;
    }

    SharedPtr<MpmSolver2D> createGranularMaterialGrid(
        std::uint64_t simulationWorldId, LayerID layerId, float left, float top,
        float width, float height, float domainLeft, float domainTop,
        float domainWidth, float domainHeight,
        int columns, int rows, float youngModulus, float poissonRatio,
        float frictionAngleRadians, float density, float gravityY,
        float damping, float supportRadiusLimit) {
        constexpr float kMaximumFrictionAngleRadians =
            80.0f * (3.14159265358979323846f / 180.0f);
        if (simulationWorldId == 0 || layerId.isNil() ||
            !std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(width) || !std::isfinite(height) ||
            width <= 0.0f || height <= 0.0f ||
            !std::isfinite(domainLeft) || !std::isfinite(domainTop) ||
            !std::isfinite(domainWidth) || !std::isfinite(domainHeight) ||
            domainWidth <= 0.0f || domainHeight <= 0.0f ||
            !std::isfinite(youngModulus) || youngModulus < 1.0f ||
            youngModulus > 1000000.0f || !std::isfinite(poissonRatio) ||
            poissonRatio < -0.9f || poissonRatio > 0.45f ||
            !std::isfinite(frictionAngleRadians) ||
            frictionAngleRadians < 0.0f ||
            frictionAngleRadians > kMaximumFrictionAngleRadians ||
            !std::isfinite(supportRadiusLimit) ||
            supportRadiusLimit <= 0.0f ||
            !std::isfinite(density) || density < 0.01f || density > 100.0f ||
            !std::isfinite(gravityY) || std::abs(gravityY) > 5000.0f ||
            !std::isfinite(damping) || damping < 0.0f || damping > 1.0f) {
            return nullptr;
        }

        // Preserve authored resolution and reject unsupported sizes instead
        // of silently turning an invalid request into a denser simulation.
        // The MPM Sand layer's persistent UI/JSON contract is 2..500 columns
        // and 2..200 rows; keep a defensive aggregate cap at this API edge.
        constexpr int kMaximumColumns = 500;
        constexpr int kMaximumRows = 200;
        constexpr std::int64_t kMaxInitialParticles = 100000;
        if (columns < 2 || columns > kMaximumColumns || rows < 2 ||
            rows > kMaximumRows ||
            static_cast<std::int64_t>(columns) * rows >
                kMaxInitialParticles) {
            return nullptr;
        }
        const int safeColumns = columns;
        const int safeRows = rows;
        const std::size_t requestedParticleBytes =
            static_cast<std::size_t>(safeColumns) *
            static_cast<std::size_t>(safeRows) * sizeof(MpmParticle2D);
        std::size_t existingPersistentParticleBytes = 0;
        const auto addParticlePayloadBytes =
            [&existingPersistentParticleBytes](std::size_t particleCount) {
                if (particleCount >
                    (kGranularWorldStateBudgetBytes -
                     existingPersistentParticleBytes) /
                        sizeof(MpmParticle2D)) {
                    return false;
                }
                existingPersistentParticleBytes +=
                    particleCount * sizeof(MpmParticle2D);
                return true;
            };
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId) {
                continue;
            }
            if (entry.solver && !addParticlePayloadBytes(
                    entry.solver->particles().capacity())) {
                return nullptr;
            }
            if (entry.baselineSnapshot && !addParticlePayloadBytes(
                    entry.baselineSnapshot->particles.capacity())) {
                return nullptr;
            }
        }
        if (requestedParticleBytes >
            (kGranularWorldStateBudgetBytes -
             existingPersistentParticleBytes) / 2u) {
            return nullptr;
        }
        const double particleSpacingX =
            static_cast<double>(width) / safeColumns;
        const double particleSpacingY =
            static_cast<double>(height) / safeRows;
        const float cellSize = static_cast<float>(std::max(
            1.0, std::min(particleSpacingX, particleSpacingY)));
        const double maxFloat =
            static_cast<double>(std::numeric_limits<float>::max());
        const double right = static_cast<double>(left) + width;
        const double bottom = static_cast<double>(top) + height;
        const double domainRight = static_cast<double>(domainLeft) + domainWidth;
        const double domainBottom = static_cast<double>(domainTop) + domainHeight;
        if (!std::isfinite(domainRight) || !std::isfinite(domainBottom) ||
            std::abs(domainRight) > maxFloat ||
            std::abs(domainBottom) > maxFloat) {
            return nullptr;
        }
        const float boundaryLeft = static_cast<float>(domainLeft);
        const float boundaryTop = static_cast<float>(domainTop);
        const float boundaryRight = static_cast<float>(domainRight);
        const float boundaryBottom = static_cast<float>(domainBottom);
        const double boundaryWidth =
            static_cast<double>(boundaryRight) - boundaryLeft;
        const double boundaryHeight =
            static_cast<double>(boundaryBottom) - boundaryTop;
        const double originX = static_cast<double>(domainLeft) - cellSize * 2.0;
        const double originY = static_cast<double>(domainTop) - cellSize * 2.0;
        const double totalVolume = static_cast<double>(width) * height;
        const double totalMass = totalVolume * density;
        const double particleVolume = totalVolume /
            (static_cast<double>(safeColumns) * safeRows);
        const double particleMass = particleVolume * density;
        const float seedCenterX = left + width * 0.5f;
        const float seedCenterY = top + height * 0.5f;
        const auto seedGeometry =
            calculateMpmParticleGridSeedGeometry2D(
                seedCenterX, seedCenterY, width, height, safeColumns,
                safeRows, density, true);
        const double shearModulus = youngModulus /
            (2.0 * (1.0 + poissonRatio));
        const double lameLambda = youngModulus * poissonRatio /
            ((1.0 + poissonRatio) * (1.0 - 2.0 * poissonRatio));
        const double longitudinalWaveSpeedSquared =
            (lameLambda + 2.0 * shearModulus) / density;
        const double cflTimeStep = 0.05 * cellSize /
            std::sqrt(longitudinalWaveSpeedSquared);
        const double solverTimeStep = std::min(1.0 / 120.0, cflTimeStep);
        constexpr int kGranularMaxSubsteps = 1024;
        const double requiredSubsteps = std::ceil((1.0 / 15.0) / solverTimeStep);
        const double minFloat =
            static_cast<double>(std::numeric_limits<float>::min());
        if (!std::isfinite(right) || !std::isfinite(bottom) ||
            !std::isfinite(domainRight) || !std::isfinite(domainBottom) ||
            !std::isfinite(boundaryLeft) || !std::isfinite(boundaryTop) ||
            !std::isfinite(boundaryRight) || !std::isfinite(boundaryBottom) ||
            !std::isfinite(boundaryWidth) || !std::isfinite(boundaryHeight) ||
            boundaryWidth < 1.0e-6 || boundaryHeight < 1.0e-6 ||
            boundaryWidth > maxFloat || boundaryHeight > maxFloat ||
            !std::isfinite(originX) || !std::isfinite(originY) ||
            !std::isfinite(totalMass) || std::abs(right) > maxFloat ||
            std::abs(bottom) > maxFloat || std::abs(domainRight) > maxFloat ||
            std::abs(domainBottom) > maxFloat || std::abs(originX) > maxFloat ||
            std::abs(originY) > maxFloat || totalVolume > maxFloat ||
            !std::isfinite(seedCenterX) || !std::isfinite(seedCenterY) ||
            !seedGeometry.valid || seedGeometry.particleCount !=
                static_cast<std::size_t>(safeColumns) * safeRows ||
            left < boundaryLeft || top < boundaryTop ||
            right > static_cast<double>(boundaryRight) ||
            bottom > static_cast<double>(boundaryBottom) ||
            totalMass > maxFloat || particleVolume < minFloat ||
            particleMass < minFloat || !std::isfinite(shearModulus) ||
            !std::isfinite(lameLambda) ||
            !std::isfinite(longitudinalWaveSpeedSquared) ||
            longitudinalWaveSpeedSquared <= 0.0 ||
            !std::isfinite(solverTimeStep) || solverTimeStep < 1.0e-6 ||
            !std::isfinite(requiredSubsteps) ||
            requiredSubsteps > kGranularMaxSubsteps ||
            shearModulus > maxFloat || std::abs(lameLambda) > maxFloat) {
            return nullptr;
        }
        const double gridWidthValue =
            std::ceil(boundaryWidth / cellSize) + 4.0;
        const double gridHeightValue =
            std::ceil(boundaryHeight / cellSize) + 4.0;
        if (!std::isfinite(gridWidthValue) || !std::isfinite(gridHeightValue) ||
            gridWidthValue > 16384.0 || gridHeightValue > 16384.0 ||
            gridWidthValue * gridHeightValue > 4194304.0) {
            return nullptr;
        }
        constexpr std::size_t kGranularWorldGridNodeBudget = 4194304u;
        std::size_t existingGridNodes = 0;
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId ||
                !entry.solver) {
                continue;
            }
            const std::size_t nodes = entry.solver->gridNodeCapacity();
            if (nodes > kGranularWorldGridNodeBudget - existingGridNodes) {
                return nullptr;
            }
            existingGridNodes += nodes;
        }
        const auto requestedGridColumns = static_cast<std::size_t>(
            std::max(4, static_cast<int>(gridWidthValue)));
        const auto requestedGridRows = static_cast<std::size_t>(
            std::max(4, static_cast<int>(gridHeightValue)));
        const float solverGridOriginX = domainLeft - cellSize * 2.0f;
        const float solverGridOriginY = domainTop - cellSize * 2.0f;
        const float safeGridMinX = solverGridOriginX + 2.0f * cellSize;
        const float safeGridMinY = solverGridOriginY + 2.0f * cellSize;
        const float safeGridMaxX = solverGridOriginX +
            (static_cast<float>(requestedGridColumns) - 2.0f) * cellSize;
        const float safeGridMaxY = solverGridOriginY +
            (static_cast<float>(requestedGridRows) - 2.0f) * cellSize;
        if (!std::isfinite(solverGridOriginX) ||
            !std::isfinite(solverGridOriginY) ||
            !std::isfinite(safeGridMinX) || !std::isfinite(safeGridMinY) ||
            !std::isfinite(safeGridMaxX) || !std::isfinite(safeGridMaxY) ||
            !seedGeometry.valid ||
            seedGeometry.particleCount !=
                static_cast<std::size_t>(safeColumns) * safeRows ||
            safeGridMinX > boundaryLeft || safeGridMinY > boundaryTop ||
            safeGridMaxX < boundaryRight || safeGridMaxY < boundaryBottom ||
            seedGeometry.startX < safeGridMinX ||
            seedGeometry.startY < safeGridMinY ||
            seedGeometry.lastX > safeGridMaxX ||
            seedGeometry.lastY > safeGridMaxY ||
            seedGeometry.startX < boundaryLeft ||
            seedGeometry.startY < boundaryTop ||
            seedGeometry.lastX > boundaryRight ||
            seedGeometry.lastY > boundaryBottom) {
            return nullptr;
        }
        const std::size_t requestedGridNodes =
            requestedGridColumns * requestedGridRows;
        const std::size_t remainingGridNodeBudget =
            kGranularWorldGridNodeBudget - existingGridNodes;
        if (requestedGridNodes > remainingGridNodeBudget) {
            return nullptr;
        }
        // Reserve a bounded fourfold node budget while the solver is created.
        // Grid expansion during simulation then changes only active geometry;
        // it never grows the backing node vector in the frame path.
        constexpr std::size_t kGranularGridReserveFactor = 4u;
        const std::size_t requestedGridCapacity =
            requestedGridNodes * kGranularGridReserveFactor;
        const std::size_t reservedGridNodeCapacity = std::min(
            requestedGridCapacity, remainingGridNodeBudget);

        // Reconfiguration keeps the current solver alive until the new solver
        // and baseline are ready to commit. Reserve room for that transient
        // particle payload too, evicting the oldest world checkpoints first.
        const std::size_t incomingPersistentParticleBytes =
            requestedParticleBytes * 2u;
        if (granularMaterialSnapshots_.find(simulationWorldId) !=
                granularMaterialSnapshots_.end() &&
            !trimGranularWorldSnapshots(
                simulationWorldId, incomingPersistentParticleBytes)) {
            return nullptr;
        }

        auto solver = makeShared<MpmSolver2D>();
        if (!solver->setGrid(cellSize,
                             static_cast<int>(requestedGridColumns),
                             static_cast<int>(requestedGridRows)) ||
            !solver->reserveGridNodeCapacity(reservedGridNodeCapacity) ||
            !solver->setGridOrigin(solverGridOriginX, solverGridOriginY)) {
            return nullptr;
        }
        solver->setBoundaryFriction(0.25f);
        solver->setMaterial(youngModulus, poissonRatio);
        if (!solver->setConstitutiveModel(
                MpmConstitutiveModel::GranularDruckerPrager) ||
            !solver->setGranularFrictionAngle(frictionAngleRadians) ||
            !solver->setGranularFloor(boundaryBottom)) {
            return nullptr;
        }
        solver->setFractureEnabled(false);
        solver->setGravity(0.0f, gravityY);
        solver->setTimeStep(static_cast<float>(solverTimeStep));
        solver->setMaxSubsteps(kGranularMaxSubsteps);
        solver->setDamping(damping);
        if (!solver->setGranularSupportRadiusLimit(supportRadiusLimit)) {
            return nullptr;
        }
        solver->addParticlesGrid(seedCenterX, seedCenterY,
                                 width, height, safeColumns, safeRows, density);
        if (solver->particles().size() !=
            static_cast<std::size_t>(safeColumns) * safeRows) {
            return nullptr;
        }
        const bool validSeedParticles = std::all_of(
            solver->particles().begin(), solver->particles().end(),
            [](const MpmParticle2D& particle) {
                return particle.active && std::isfinite(particle.pos.x) &&
                       std::isfinite(particle.pos.y) &&
                       std::isfinite(particle.mass) && particle.mass > 0.0f &&
                       std::isfinite(particle.volume0) &&
                       particle.volume0 > 0.0f;
            });
        if (!validSeedParticles) return nullptr;
        const std::size_t actualParticleCapacity =
            solver->particles().capacity();
        const std::size_t actualIncomingPersistentParticleBytes =
            actualParticleCapacity * sizeof(MpmParticle2D) +
            requestedParticleBytes;
        if (granularMaterialSnapshots_.find(simulationWorldId) !=
                granularMaterialSnapshots_.end() &&
            !trimGranularWorldSnapshots(
                simulationWorldId,
                actualIncomingPersistentParticleBytes)) {
            return nullptr;
        }
        MpmSnapshot2D baselineSnapshot = solver->snapshot();
        const std::size_t baselineParticleCapacity =
            baselineSnapshot.particles.capacity();
        const std::size_t actualIncomingParticleBytes =
            (actualParticleCapacity + baselineParticleCapacity) *
            sizeof(MpmParticle2D);
        if (granularMaterialSnapshots_.find(simulationWorldId) !=
                granularMaterialSnapshots_.end() &&
            !trimGranularWorldSnapshots(
                simulationWorldId, actualIncomingParticleBytes)) {
            return nullptr;
        }
        if (actualParticleCapacity >
            (kGranularWorldStateBudgetBytes -
             existingPersistentParticleBytes) / sizeof(MpmParticle2D) ||
            baselineParticleCapacity >
                (kGranularWorldStateBudgetBytes -
                 existingPersistentParticleBytes -
                 actualParticleCapacity * sizeof(MpmParticle2D)) /
                    sizeof(MpmParticle2D)) {
            return nullptr;
        }
        setGranularMaterialSolver(simulationWorldId, layerId, solver,
                                  std::move(baselineSnapshot));
        return solver;
    }

    SharedPtr<MpmSolver2D> getGranularMaterialSolver(
        std::uint64_t simulationWorldId, LayerID layerId) {
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId == simulationWorldId &&
                entry.layerId == layerId) {
                return entry.solver;
            }
        }
        return nullptr;
    }

    void registerGranularMaterialCollider(std::uint64_t simulationWorldId,
                                         LayerID layerId,
                                         const MpmCollider2D& collider) {
        if (auto solver = getGranularMaterialSolver(simulationWorldId, layerId)) {
            solver->addCollider(collider);
        }
    }

    void clearGranularMaterialColliders(std::uint64_t simulationWorldId,
                                       LayerID layerId) {
        if (auto solver = getGranularMaterialSolver(simulationWorldId, layerId)) {
            solver->clearColliders();
        }
    }

    void unregisterGranularMaterialSolver(std::uint64_t simulationWorldId,
                                         LayerID layerId) {
        granularMaterialSolvers_.removeIf(
            [simulationWorldId, layerId](const GranularMaterialSolverEntry& entry) {
                return entry.simulationWorldId == simulationWorldId &&
                       entry.layerId == layerId;
            });
        granularMaterialSnapshots_.erase(simulationWorldId);
        const bool hasWorldSolver = std::any_of(
            granularMaterialSolvers_.begin(), granularMaterialSolvers_.end(),
            [simulationWorldId](const GranularMaterialSolverEntry& entry) {
                return entry.simulationWorldId == simulationWorldId;
            });
        if (!hasWorldSolver) {
            granularMaterialWorldClocks_.removeIf(
                [simulationWorldId](const GranularMaterialWorldClock& clock) {
                    return clock.simulationWorldId == simulationWorldId;
                });
        } else {
            for (auto& clock : granularMaterialWorldClocks_) {
                if (clock.simulationWorldId == simulationWorldId) {
                    clock.requiresReplay = true;
                    clock.replayAnchorFrame = 0;
                    break;
                }
            }
            // Unregistering one layer invalidates the shared checkpoint set.
            // Replay the remaining solvers from their baselines before the
            // next incremental frame, then recreate their cache nodes.
            auto& worldSnapshots = granularMaterialSnapshots_[simulationWorldId];
            for (const auto& entry : granularMaterialSolvers_) {
                if (entry.simulationWorldId == simulationWorldId) {
                    worldSnapshots[entry.layerId];
                }
            }
        }
    }

    void setGranularMaterialSolverActive(std::uint64_t simulationWorldId,
                                         LayerID layerId, bool active) {
        if (auto* entry = findGranularMaterialSolver(simulationWorldId, layerId)) {
            entry->simulationActive = active;
        }
    }

    void unregisterGranularMaterialWorld(std::uint64_t simulationWorldId) {
        if (simulationWorldId == 0) return;
        granularMaterialSolvers_.removeIf(
            [simulationWorldId](const GranularMaterialSolverEntry& entry) {
                return entry.simulationWorldId == simulationWorldId;
            });
        granularMaterialSnapshots_.erase(simulationWorldId);
        granularMaterialWorldClocks_.removeIf(
            [simulationWorldId](const GranularMaterialWorldClock& clock) {
                return clock.simulationWorldId == simulationWorldId;
            });
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

    NamedVector<MaterialFractureEvent> takeMaterialFractureEvents() {
        auto events = pendingMaterialFractureEvents_;
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
    NamedVector<SharedPtr<RigidBody2D>> createRigidBoxes(
        LayerID layerId, const NamedVector<QVector2D>& positions,
        float width, float height,
        float density = 1.0f, float friction = 0.3f, float restitution = 0.2f) {
        auto world = getRigidWorld(layerId);
        if (!world) world = createRigidWorld(layerId);
        NamedVector<SharedPtr<RigidBody2D>> out{
            ContainerName{"Physics.RigidBoxes"}};
        out.reserve(positions.size());
        for (const auto& p : positions) {
            out.push_back(world->addDynamicBox(p.x(), p.y(), width, height, density, friction, restitution));
        }
        return out;
    }

    NamedVector<SharedPtr<RigidBody2D>> createRigidCircles(
        LayerID layerId, const NamedVector<QVector2D>& positions,
        float radius, float density = 1.0f,
        float friction = 0.3f, float restitution = 0.2f) {
        auto world = getRigidWorld(layerId);
        if (!world) world = createRigidWorld(layerId);
        NamedVector<SharedPtr<RigidBody2D>> out{
            ContainerName{"Physics.RigidCircles"}};
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
        if (auto* constants = findBoidsConstants(layerId)) {
            *constants = makeShared<GpuBoidConstants>(c);
            return;
        }
        boidsConstants_.add(
            BoidsConstantsEntry{layerId, makeShared<GpuBoidConstants>(c)});
    }

    std::optional<GpuBoidConstants> getBoidsConstants(LayerID layerId) const {
        const auto* constants = findBoidsConstants(layerId);
        if (constants && *constants) return **constants;
        return std::nullopt;
    }

    void unregisterBoids(LayerID layerId) {
        boidsConstants_.removeIf(
            [layerId](const BoidsConstantsEntry& entry) {
                return entry.layerId == layerId;
            });
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
            v.ppx = v.px; v.ppy = v.py; v.ppz = v.pz;
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
    NamedVector<SoftBodyCollider> getSoftBodyColliders(LayerID layerId) const {
        auto it = softBodyColliders_.find(layerId);
        if (it != softBodyColliders_.end()) {
            return it->second;
        }
        return NamedVector<SoftBodyCollider>{
            ContainerName{"Physics.SoftBodyColliders"}};
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
        const auto triangleIndices = solver.getGridTriangleIndices();
        mesh.indices.reserve(triangleIndices.size());
        for (const auto index : triangleIndices) {
            mesh.indices.push_back(index);
        }
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
    void capturePhysicsSnapshots(int64_t frame,
                                 std::uint64_t simulationWorldId = 0) {
        for (const auto& [layerId, solver] : fluidSolvers_) {
            if (!solver) continue;
            auto& snapshots = fluidSnapshots_[layerId];
            if (snapshots.empty()) {
                snapshots.reserve(timelineSettings_.maxCachedFrames);
            }
            snapshots[frame] = solver->snapshot();
        }
        for (const auto& [layerId, solver] : softBodies_) {
            if (!solver) continue;
            auto& snapshots = softBodySnapshots_[layerId];
            if (snapshots.empty()) {
                snapshots.reserve(timelineSettings_.maxCachedFrames);
            }
            snapshots[frame] = solver->snapshot();
        }
        for (const auto& [layerId, solver] : cloth3DBodies_) {
            if (!solver) continue;
            auto& snapshots = cloth3DSnapshots_[layerId];
            if (snapshots.empty()) {
                snapshots.reserve(timelineSettings_.maxCachedFrames);
            }
            snapshots[frame] = solver->snapshot();
        }
        for (const auto& entry : materialSolvers_) {
            const auto& layerId = entry.layerId;
            const auto& solver = entry.solver;
            if (!solver) continue;
            auto& snapshots = materialSnapshots_[layerId];
            if (snapshots.empty()) {
                snapshots.reserve(timelineSettings_.maxCachedFrames);
            }
            snapshots[frame] = makeShared<MpmSnapshot2D>(solver->snapshot());
        }
        if (simulationWorldId != 0) {
            captureGranularMaterialSnapshots(frame, simulationWorldId);
        }
        for (const auto& [layerId, world] : rigidWorlds_) {
            if (!world) continue;
            auto& snapshots = rigidSnapshots_[layerId];
            if (snapshots.empty()) {
                snapshots.reserve(timelineSettings_.maxCachedFrames);
            }
            snapshots[frame] = world->snapshot();
        }
        for (const auto& [compositionId, world] : compositionRigidWorlds_) {
            if (!world) continue;
            auto& snapshots = compositionRigidSnapshots_[compositionId];
            if (snapshots.empty()) {
                snapshots.reserve(timelineSettings_.maxCachedFrames);
            }
            snapshots[frame] = world->snapshot();
        }
        trimPhysicsSnapshots();
    }

    bool captureGranularMaterialSnapshots(int64_t frame,
                                          std::uint64_t simulationWorldId,
                                          bool forceCheckpoint = false) {
        if (simulationWorldId == 0 ||
            (!forceCheckpoint &&
             frame % kGranularMaterialSnapshotInterval != 0)) return false;
        const std::size_t frameCheckpointLimit = std::max<std::size_t>(
            1, (timelineSettings_.maxCachedFrames +
                static_cast<std::size_t>(kGranularMaterialSnapshotInterval) - 1) /
                   static_cast<std::size_t>(kGranularMaterialSnapshotInterval));
        auto worldSnapshots = granularMaterialSnapshots_.find(
            simulationWorldId);
        if (worldSnapshots == granularMaterialSnapshots_.end()) return false;
        std::size_t incomingSnapshotBytes = 0;
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId || !entry.solver) {
                continue;
            }
            const std::size_t particleCapacity =
                entry.solver->particles().capacity();
            if (particleCapacity >
                (std::numeric_limits<std::size_t>::max() -
                 incomingSnapshotBytes) / sizeof(MpmParticle2D)) {
                incomingSnapshotBytes =
                    std::numeric_limits<std::size_t>::max();
                break;
            }
            incomingSnapshotBytes +=
                particleCapacity * sizeof(MpmParticle2D);
        }
        if (incomingSnapshotBytes > kGranularWorldStateBudgetBytes) return false;
        const auto worldClock = std::find_if(
            granularMaterialWorldClocks_.begin(),
            granularMaterialWorldClocks_.end(),
            [simulationWorldId](const GranularMaterialWorldClock& clock) {
                return clock.simulationWorldId == simulationWorldId;
            });
        if (worldClock == granularMaterialWorldClocks_.end()) return false;

        int64_t reusableCheckpointFrame =
            std::numeric_limits<int64_t>::min();
        const auto hasReusableCheckpointStorage =
            [&](int64_t candidateFrame) {
                for (const auto& solverEntry : granularMaterialSolvers_) {
                    if (solverEntry.simulationWorldId != simulationWorldId ||
                        !solverEntry.solver) {
                        continue;
                    }
                    const auto layerSnapshots = worldSnapshots->second.find(
                        solverEntry.layerId);
                    if (layerSnapshots == worldSnapshots->second.end()) {
                        return false;
                    }
                    const auto checkpoint = layerSnapshots->second.find(
                        candidateFrame);
                    if (checkpoint == layerSnapshots->second.end() ||
                        !checkpoint->second ||
                        checkpoint->second->particles.capacity() <
                            solverEntry.solver->particles().size() ||
                        checkpoint->second->fracturedIndices.capacity() <
                            solverEntry.solver->fractureEventCount()) {
                        return false;
                    }
                }
                return true;
            };
        for (const auto& layerEntry : worldSnapshots->second) {
            for (const auto& checkpoint : layerEntry.second) {
                if (checkpoint.first == frame ||
                    checkpoint.first == worldClock->replayAnchorFrame) {
                    continue;
                }
                if (hasReusableCheckpointStorage(checkpoint.first)) {
                    reusableCheckpointFrame = checkpoint.first;
                    break;
                }
            }
            if (reusableCheckpointFrame !=
                std::numeric_limits<int64_t>::min()) {
                break;
            }
        }
        struct PartialCheckpointRollback {
            decltype(worldSnapshots->second)& layerSnapshots;
            int64_t frame;
            bool committed = false;
            ~PartialCheckpointRollback() {
                if (committed) return;
                for (auto& layerEntry : layerSnapshots) {
                    layerEntry.second.erase(frame);
                }
            }
        } rollback{worldSnapshots->second, frame};
        const std::size_t reservedIncomingBytes =
            reusableCheckpointFrame == std::numeric_limits<int64_t>::min()
                ? incomingSnapshotBytes
                : 0;
        if (!trimGranularWorldSnapshots(simulationWorldId,
                                        reservedIncomingBytes)) {
            return false;
        }
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId || !entry.solver) {
                continue;
            }
            auto layerSnapshots = worldSnapshots->second.find(entry.layerId);
            if (layerSnapshots == worldSnapshots->second.end()) return false;
            auto& snapshots = layerSnapshots->second;
            if (snapshots.empty()) snapshots.reserve(frameCheckpointLimit);
            SharedPtr<MpmSnapshot2D> snapshotStorage;
            const auto existingFrame = snapshots.find(frame);
            if (existingFrame != snapshots.end()) {
                snapshotStorage = existingFrame->second;
            } else if (reusableCheckpointFrame !=
                       std::numeric_limits<int64_t>::min()) {
                const auto reusableFrame = snapshots.find(
                    reusableCheckpointFrame);
                if (reusableFrame != snapshots.end()) {
                    snapshotStorage = std::move(reusableFrame->second);
                    snapshots.erase(reusableFrame);
                }
            }
            if (!snapshotStorage) {
                snapshotStorage = makeShared<MpmSnapshot2D>();
            }
            entry.solver->snapshot(*snapshotStorage);
            snapshotStorage->physicsWorldAccumulator = worldClock->accumulator;
            snapshots[frame] = snapshotStorage;
            const auto addIncomingArrayBytes =
                [](std::size_t count, std::size_t elementSize,
                   std::size_t& total) {
                    if (elementSize != 0 && count >
                        (std::numeric_limits<std::size_t>::max() - total) /
                            elementSize) {
                        return false;
                    }
                    total += count * elementSize;
                    return true;
                };
            std::size_t actualSnapshotBytes = 0;
            if (!addIncomingArrayBytes(snapshotStorage->particles.capacity(),
                                       sizeof(MpmParticle2D),
                                       actualSnapshotBytes) ||
                !addIncomingArrayBytes(snapshotStorage->fracturedIndices.capacity(),
                                       sizeof(int), actualSnapshotBytes) ||
                !trimGranularWorldSnapshots(simulationWorldId,
                                            0)) return false;
            const bool hasReplayAnchor = snapshots.find(
                worldClock->replayAnchorFrame) != snapshots.end();
            const std::size_t retainedFrameLimit =
                frameCheckpointLimit + (hasReplayAnchor ? 1u : 0u);
            while (snapshots.size() > retainedFrameLimit) {
                auto oldest = snapshots.begin();
                if (oldest->first == worldClock->replayAnchorFrame) {
                    ++oldest;
                    if (oldest == snapshots.end()) break;
                }
                snapshots.erase(oldest);
            }
        }
        if (!trimGranularWorldSnapshots(simulationWorldId)) return false;
        rollback.committed = true;
        if (forceCheckpoint) {
            worldClock->replayAnchorFrame = frame;
        }
        return true;
    }

    void invalidateGranularMaterialSnapshots(
        std::uint64_t simulationWorldId,
        bool resetWorldClock = false) {
        if (simulationWorldId == 0) return;
        granularMaterialSnapshots_.erase(simulationWorldId);
        if (resetWorldClock) {
            setGranularWorldClockAccumulator(simulationWorldId, 0.0f);
            for (auto& clock : granularMaterialWorldClocks_) {
                if (clock.simulationWorldId == simulationWorldId) {
                    clock.requiresReplay = true;
                    clock.replayAnchorFrame =
                        std::numeric_limits<int64_t>::min();
                    break;
                }
            }
        }
        auto& worldSnapshots = granularMaterialSnapshots_[simulationWorldId];
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId == simulationWorldId) {
                worldSnapshots[entry.layerId];
            }
        }
    }

    bool trimGranularWorldSnapshots(
        std::uint64_t simulationWorldId,
        std::size_t reservedIncomingBytes = 0) {
        if (reservedIncomingBytes > kGranularWorldStateBudgetBytes) {
            return false;
        }
        const auto worldSnapshots =
            granularMaterialSnapshots_.find(simulationWorldId);
        if (worldSnapshots == granularMaterialSnapshots_.end()) return false;
        const auto totalBytes = [&]() {
            std::size_t total = 0;
            const auto addArrayBytes = [&total](std::size_t count,
                                                std::size_t elementSize) {
                if (elementSize != 0 && count >
                    (std::numeric_limits<std::size_t>::max() - total) /
                        elementSize) {
                    total = std::numeric_limits<std::size_t>::max();
                    return;
                }
                total += count * elementSize;
            };
            for (const auto& solverEntry : granularMaterialSolvers_) {
                if (solverEntry.simulationWorldId != simulationWorldId ||
                    !solverEntry.solver) {
                    continue;
                }
                addArrayBytes(solverEntry.solver->particles().capacity(),
                              sizeof(MpmParticle2D));
                if (!solverEntry.baselineSnapshot) continue;
                addArrayBytes(solverEntry.baselineSnapshot->particles.capacity(),
                              sizeof(MpmParticle2D));
                addArrayBytes(
                    solverEntry.baselineSnapshot->fracturedIndices.capacity(),
                    sizeof(int));
            }
            for (const auto& layerEntry : worldSnapshots->second) {
                for (const auto& snapshotEntry : layerEntry.second) {
                    if (!snapshotEntry.second) continue;
                    addArrayBytes(snapshotEntry.second->particles.capacity(),
                                  sizeof(MpmParticle2D));
                    addArrayBytes(
                        snapshotEntry.second->fracturedIndices.capacity(),
                        sizeof(int));
                }
            }
            return total;
        };
        const std::size_t retainedBudgetBytes =
            kGranularWorldStateBudgetBytes - reservedIncomingBytes;
        while (totalBytes() > retainedBudgetBytes) {
            int64_t oldestFrame = std::numeric_limits<int64_t>::max();
            int64_t replayAnchorFrame =
                std::numeric_limits<int64_t>::min();
            for (const auto& clock : granularMaterialWorldClocks_) {
                if (clock.simulationWorldId == simulationWorldId) {
                    replayAnchorFrame = clock.replayAnchorFrame;
                    break;
                }
            }
            for (const auto& layerEntry : worldSnapshots->second) {
                for (const auto& snapshotEntry : layerEntry.second) {
                    if (snapshotEntry.first == replayAnchorFrame) continue;
                    oldestFrame = std::min(oldestFrame, snapshotEntry.first);
                    break;
                }
            }
            if (oldestFrame == std::numeric_limits<int64_t>::max()) break;
            for (auto& layerEntry : worldSnapshots->second) {
                layerEntry.second.erase(oldestFrame);
            }
        }
        return totalBytes() <= retainedBudgetBytes;
    }

    bool resetGranularMaterialToBaseline(std::uint64_t simulationWorldId) {
        if (simulationWorldId == 0) return false;
        bool hasSolver = false;
        const bool hasWorldClock = std::any_of(
            granularMaterialWorldClocks_.begin(),
            granularMaterialWorldClocks_.end(),
            [simulationWorldId](const GranularMaterialWorldClock& clock) {
                return clock.simulationWorldId == simulationWorldId;
            });
        if (!hasWorldClock) return false;
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId || !entry.solver) continue;
            hasSolver = true;
            if (!entry.baselineSnapshot ||
                !entry.solver->canRestoreSnapshot(*entry.baselineSnapshot)) {
                return false;
            }
        }
        if (!hasSolver) return false;
        for (auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId == simulationWorldId && entry.solver) {
                entry.solver->restoreSnapshot(*entry.baselineSnapshot);
            }
        }
        for (auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                clock.accumulator = 0.0f;
                return true;
            }
        }
        return false;
    }

    // Backward-compatible entry point. Existing callers that only know the
    // old name now participate in the same mixed-solver cache.
    void captureSoftBodySnapshots(int64_t frame,
                                  std::uint64_t simulationWorldId = 0) {
        capturePhysicsSnapshots(frame, simulationWorldId);
    }

    bool restorePhysicsSnapshots(int64_t frame,
                                 std::uint64_t simulationWorldId = 0) {
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
        bool hasGranularSnapshot = false;
        float granularWorldAccumulator = 0.0f;
        if (simulationWorldId != 0) {
            for (const auto& entry : granularMaterialSolvers_) {
                if (entry.simulationWorldId != simulationWorldId || !entry.solver) {
                    continue;
                }
                const auto worldSnapshots =
                    granularMaterialSnapshots_.find(simulationWorldId);
                if (worldSnapshots == granularMaterialSnapshots_.end()) return false;
                const auto layerSnapshots =
                    worldSnapshots->second.find(entry.layerId);
                if (layerSnapshots == worldSnapshots->second.end()) return false;
                const auto snapshotIt = layerSnapshots->second.find(frame);
                if (snapshotIt == layerSnapshots->second.end() || !snapshotIt->second ||
                    !entry.solver->canRestoreSnapshot(*snapshotIt->second)) {
                    return false;
                }
                if (hasGranularSnapshot &&
                    snapshotIt->second->physicsWorldAccumulator !=
                        granularWorldAccumulator) {
                    return false;
                }
                granularWorldAccumulator =
                    snapshotIt->second->physicsWorldAccumulator;
                hasGranularSnapshot = true;
            }
            if (hasGranularSnapshot) {
                const bool hasWorldClock = std::any_of(
                    granularMaterialWorldClocks_.begin(),
                    granularMaterialWorldClocks_.end(),
                    [simulationWorldId](
                        const GranularMaterialWorldClock& clock) {
                        return clock.simulationWorldId == simulationWorldId;
                    });
                if (!hasWorldClock) return false;
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
        if (simulationWorldId != 0) {
            for (auto& entry : granularMaterialSolvers_) {
                if (entry.simulationWorldId != simulationWorldId || !entry.solver) {
                    continue;
                }
                const auto snapshot = granularMaterialSnapshots_
                    .at(simulationWorldId).at(entry.layerId).at(frame);
                entry.solver->restoreSnapshot(*snapshot);
            }
            if (hasGranularSnapshot) {
                setGranularWorldClockAccumulator(
                    simulationWorldId, granularWorldAccumulator);
            }
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

    bool restoreGranularMaterialSnapshots(
        int64_t frame, std::uint64_t simulationWorldId,
        int64_t* restoredFrame = nullptr) {
        if (simulationWorldId == 0) return false;
        int64_t bestFrame = std::numeric_limits<int64_t>::min();
        float bestWorldAccumulator = 0.0f;
        const auto worldSnapshots =
            granularMaterialSnapshots_.find(simulationWorldId);
        if (worldSnapshots == granularMaterialSnapshots_.end()) return false;
        const GranularMaterialSolverEntry* anchorSolver = nullptr;
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId || !entry.solver) {
                continue;
            }
            anchorSolver = &entry;
            break;
        }
        if (!anchorSolver) return false;
        const auto anchorSnapshots =
            worldSnapshots->second.find(anchorSolver->layerId);
        if (anchorSnapshots == worldSnapshots->second.end()) return false;
        const auto& candidates = anchorSnapshots->second;
        // PhysicsOrderedRegistry keeps frame keys sorted; newest-first avoids
        // validating every particle in older snapshots during a normal seek.
        for (auto candidateIt = candidates.end();
             candidateIt != candidates.begin();) {
            --candidateIt;
            const auto& candidate = *candidateIt;
            if (candidate.first > frame || !candidate.second ||
                !anchorSolver->solver->canRestoreSnapshot(*candidate.second)) {
                continue;
            }
            bool commonCheckpoint = true;
            const float checkpointWorldAccumulator =
                candidate.second->physicsWorldAccumulator;
            for (const auto& other : granularMaterialSolvers_) {
                if (other.simulationWorldId != simulationWorldId ||
                    !other.solver || other.layerId == anchorSolver->layerId) {
                    continue;
                }
                const auto otherSnapshots =
                    worldSnapshots->second.find(other.layerId);
                if (otherSnapshots == worldSnapshots->second.end()) {
                    commonCheckpoint = false;
                    break;
                }
                const auto otherSnapshot =
                    otherSnapshots->second.find(candidate.first);
                if (otherSnapshot == otherSnapshots->second.end() ||
                    !otherSnapshot->second ||
                    otherSnapshot->second->physicsWorldAccumulator !=
                        checkpointWorldAccumulator ||
                    !other.solver->canRestoreSnapshot(*otherSnapshot->second)) {
                    commonCheckpoint = false;
                    break;
                }
            }
            if (commonCheckpoint) {
                bestFrame = candidate.first;
                bestWorldAccumulator = checkpointWorldAccumulator;
                break;
            }
        }
        if (bestFrame == std::numeric_limits<int64_t>::min()) {
            return false;
        }
        const bool hasWorldClock = std::any_of(
            granularMaterialWorldClocks_.begin(),
            granularMaterialWorldClocks_.end(),
            [simulationWorldId](const GranularMaterialWorldClock& clock) {
                return clock.simulationWorldId == simulationWorldId;
            });
        if (!hasWorldClock) return false;
        for (auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId != simulationWorldId || !entry.solver) {
                continue;
            }
            const auto snapshot = granularMaterialSnapshots_
                .at(simulationWorldId).at(entry.layerId).at(bestFrame);
            entry.solver->restoreSnapshot(*snapshot);
        }
        setGranularWorldClockAccumulator(
            simulationWorldId, bestWorldAccumulator);
        for (auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                // Seeking before a pinned rate-change anchor makes that
                // anchor unusable as a continuation point. Drop the pin so
                // later captures cannot preserve a stale branch.
                if (bestFrame < clock.replayAnchorFrame) {
                    clock.replayAnchorFrame =
                        std::numeric_limits<int64_t>::min();
                }
                break;
            }
        }
        if (restoredFrame) *restoredFrame = bestFrame;
        return true;
    }

    // Step only the registered 2D soft bodies.  Layer-owned grids are
    // simulation-clock driven just like the shared rigid world, so playback
    // needs a narrow entry point that advances them without also waking up
    // the solver families that have never run on this path.
    void advanceSoftBodyFrame(float dt) {
        if (dt <= 0.0f || lodSettings_.level == PhysicsLODLevel::Frozen) return;
        updateSoftBodies(dt);
    }

    // Send every soft body back to the shape it was built with.  A seek to a
    // frame with no cached snapshot must not leave the previous solution in
    // the new clock, so this is the fallback for a rewind.
    void resetSoftBodiesToRestState() {
        for (auto& [id, sb] : softBodies_) {
            if (sb) sb->resetToRestState();
        }
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
        
        updateSoftBodies(simulationDt);

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
        granularMaterialSolvers_.clear();
        granularMaterialWorldClocks_.clear();
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
    static constexpr std::size_t kGranularWorldStateBudgetBytes =
        64u * 1024u * 1024u;

    // Shared by update() and advanceSoftBodyFrame() so both paths apply the
    // same LOD overrides, collider injection and authored gravity contract.
    void updateSoftBodies(float simulationDt) {
        for (auto& [id, sb] : softBodies_) {
            if (!sb) continue;
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
            // Each soft body owns its authored gravity and air drag.  The
            // global arguments remain for legacy callers and other solver
            // families, but must not overwrite per-layer soft-body settings.
            sb->update(simulationDt);
        }
    }

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
        for (auto& worldEntry : granularMaterialSnapshots_) {
            for (auto& layerEntry : worldEntry.second) {
                auto& snapshots = layerEntry.second;
                while (snapshots.size() > timelineSettings_.maxCachedFrames) {
                    snapshots.erase(snapshots.begin());
                }
            }
        }
    }

    struct MaterialSolverEntry {
        LayerID layerId;
        SharedPtr<MpmSolver2D> solver;
    };

    struct GranularMaterialSolverEntry {
        std::uint64_t simulationWorldId = 0;
        LayerID layerId;
        SharedPtr<MpmSolver2D> solver;
        SharedPtr<MpmSnapshot2D> baselineSnapshot;
        bool simulationActive = true;
    };

    struct GranularMaterialWorldClock {
        std::uint64_t simulationWorldId = 0;
        float accumulator = 0.0f;
        // Solver registration/reconfiguration replaces its baseline and
        // invalidates the shared cache. The Composition must replay from that
        // baseline to its live frame before the next incremental advance.
        bool requiresReplay = false;
        int64_t replayAnchorFrame = std::numeric_limits<int64_t>::min();
    };

    struct BoidsConstantsEntry {
        LayerID layerId;
        SharedPtr<GpuBoidConstants> constants;
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

    GranularMaterialSolverEntry* findGranularMaterialSolver(
        std::uint64_t simulationWorldId, LayerID layerId) {
        for (auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId == simulationWorldId &&
                entry.layerId == layerId) {
                return &entry;
            }
        }
        return nullptr;
    }

    void setGranularWorldClockAccumulator(std::uint64_t simulationWorldId,
                                          float accumulator) {
        constexpr float kMaximumGranularClockBacklog = 102.4f;
        const float restoredAccumulator = std::isfinite(accumulator)
            ? std::clamp(accumulator, 0.0f, kMaximumGranularClockBacklog)
            : 0.0f;
        for (auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                clock.accumulator = restoredAccumulator;
                return;
            }
        }
    }

    void setGranularMaterialSolver(std::uint64_t simulationWorldId,
                                   LayerID layerId,
                                   SharedPtr<MpmSolver2D> solver,
                                   MpmSnapshot2D baselineSnapshot) {
        granularMaterialSnapshots_.erase(simulationWorldId);
        bool hasWorldClock = false;
        for (auto& clock : granularMaterialWorldClocks_) {
            if (clock.simulationWorldId == simulationWorldId) {
                clock.accumulator = 0.0f;
                clock.requiresReplay = true;
                clock.replayAnchorFrame = 0;
                hasWorldClock = true;
                break;
            }
        }
        if (!hasWorldClock) {
            granularMaterialWorldClocks_.add(
                GranularMaterialWorldClock{
                    simulationWorldId, 0.0f, true, 0});
        }
        if (auto* existing =
                findGranularMaterialSolver(simulationWorldId, layerId)) {
            existing->solver = std::move(solver);
            existing->baselineSnapshot = makeShared<MpmSnapshot2D>(
                std::move(baselineSnapshot));
        } else {
            granularMaterialSolvers_.add(GranularMaterialSolverEntry{
                simulationWorldId, layerId, solver,
                makeShared<MpmSnapshot2D>(std::move(baselineSnapshot))});
        }
        // Registration runs during Composition setup, before Render Queue
        // workers start. Precreate every per-layer cache node here so worker
        // snapshot capture only performs lookups on shared registries.
        auto& worldSnapshots = granularMaterialSnapshots_[simulationWorldId];
        for (const auto& entry : granularMaterialSolvers_) {
            if (entry.simulationWorldId == simulationWorldId) {
                worldSnapshots[entry.layerId];
            }
        }
    }

    void advanceGranularMaterialSolvers(float frameDeltaSeconds,
                                       std::uint64_t simulationWorldId) {
        if (!std::isfinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0f ||
            simulationWorldId == 0 ||
            lodSettings_.level == PhysicsLODLevel::Frozen) {
            return;
        }
        const bool hasWorldSolver = std::any_of(
            granularMaterialSolvers_.begin(), granularMaterialSolvers_.end(),
            [simulationWorldId](const GranularMaterialSolverEntry& entry) {
                return entry.simulationWorldId == simulationWorldId &&
                       static_cast<bool>(entry.solver);
            });
        if (!hasWorldSolver) return;
        GranularMaterialWorldClock* clock = nullptr;
        for (auto& candidate : granularMaterialWorldClocks_) {
            if (candidate.simulationWorldId == simulationWorldId) {
                clock = &candidate;
                break;
            }
        }
        if (!clock) return;

        constexpr int kMaximumGranularClockStepsPerAdvance = 1024;
        constexpr float kMaximumGranularClockBacklog = 102.4f;
        const float fixedDt = timelineSettings_.fixedTimeStep;
        // Unlike the general physics catch-up clock, the granular clock keeps
        // pending Composition time up to a large fixed bound for deterministic
        // replay. Time beyond that safety bound is discarded. Size this
        // call's bounded work budget to its queued time, so low-FPS
        // compositions do not accumulate lag under the unrelated 64-step cap.
        clock->accumulator = std::min(
            clock->accumulator + frameDeltaSeconds,
            kMaximumGranularClockBacklog);
        const double queuedStepCount = std::ceil(
            static_cast<double>(clock->accumulator) / fixedDt);
        const int stepBudget = static_cast<int>(std::clamp(
            queuedStepCount, 0.0,
            static_cast<double>(kMaximumGranularClockStepsPerAdvance)));
        int steps = 0;
        while (clock->accumulator + 1.0e-7f >= fixedDt &&
               steps < stepBudget) {
            for (auto& entry : granularMaterialSolvers_) {
                if (entry.simulationWorldId != simulationWorldId ||
                    !entry.solver || !entry.simulationActive) {
                    continue;
                }
                entry.solver->update(fixedDt);
            }
            clock->accumulator = std::max(
                0.0f, clock->accumulator - fixedDt);
            ++steps;
        }
    }

    SharedPtr<GpuBoidConstants>* findBoidsConstants(LayerID layerId) {
        for (auto& entry : boidsConstants_) {
            if (entry.layerId == layerId) return &entry.constants;
        }
        return nullptr;
    }

    const SharedPtr<GpuBoidConstants>* findBoidsConstants(
        LayerID layerId) const {
        for (const auto& entry : boidsConstants_) {
            if (entry.layerId == layerId) return &entry.constants;
        }
        return nullptr;
    }

    PhysicsSystem() = default;
    ~PhysicsSystem();

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    
    PhysicsOrderedRegistry<LayerID, SharedPtr<FluidSolver2D>> fluidSolvers_;
    PhysicsOrderedRegistry<LayerID,
                           PhysicsOrderedRegistry<int64_t, FluidSnapshot2D>>
        fluidSnapshots_;
    PhysicsOrderedRegistry<LayerID, SharedPtr<SoftBodySolver>> softBodies_;
    PhysicsOrderedRegistry<LayerID, NamedVector<SoftBodyCollider>>
        softBodyColliders_;
    PhysicsOrderedRegistry<LayerID,
                           PhysicsOrderedRegistry<int64_t, SoftBodySnapshot>>
        softBodySnapshots_;
    PhysicsOrderedRegistry<LayerID, SharedPtr<ClothSolver3D>> cloth3DBodies_;
    PhysicsOrderedRegistry<LayerID,
                           PhysicsOrderedRegistry<int64_t, ClothSnapshot3D>>
        cloth3DSnapshots_;
    NamedVector<MaterialSolverEntry> materialSolvers_;
    NamedVector<GranularMaterialSolverEntry> granularMaterialSolvers_{
        ContainerName{"Physics.GranularMaterialSolvers"}};
    NamedVector<GranularMaterialWorldClock> granularMaterialWorldClocks_{
        ContainerName{"Physics.GranularMaterialWorldClocks"}};
    PhysicsOrderedRegistry<
        LayerID, PhysicsOrderedRegistry<int64_t, SharedPtr<MpmSnapshot2D>>>
        materialSnapshots_;
    PhysicsOrderedRegistry<std::uint64_t,
        PhysicsOrderedRegistry<LayerID,
            PhysicsOrderedRegistry<int64_t, SharedPtr<MpmSnapshot2D>>>>
        granularMaterialSnapshots_;
    NamedVector<MaterialFractureEvent> pendingMaterialFractureEvents_;
    PhysicsOrderedRegistry<LayerID, SharedPtr<Physics2D>> rigidWorlds_;
    PhysicsOrderedRegistry<LayerID,
                           PhysicsOrderedRegistry<int64_t, Physics2DSnapshot>>
        rigidSnapshots_;
    PhysicsOrderedRegistry<CompositionID, SharedPtr<Physics2D>>
        compositionRigidWorlds_;
    PhysicsOrderedRegistry<
        CompositionID, PhysicsOrderedRegistry<int64_t, Physics2DSnapshot>>
        compositionRigidSnapshots_;
#ifdef ARTIFACT_ENABLE_PYRO
    PhysicsOrderedRegistry<LayerID, SharedPtr<PyroSimulation>> pyroSimulations_;
#endif
    // Keep Graphics.BoidsCompute ownership out of std associative nodes. MSVC
    // 14.51 ICEs while importing that template instantiation through an IFC.
    NamedVector<BoidsConstantsEntry> boidsConstants_{
        ContainerName{"PhysicsSystem.BoidsConstants"}};
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
