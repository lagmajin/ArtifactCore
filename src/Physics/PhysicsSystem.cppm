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
#include <optional>

export module Physics.System;

import Physics.Fluid;
import Physics2D;
import Physics.SoftBody;
import Physics.Mpm2D;
import Memory.TrackedPtr;
import Utils.Id;

namespace ArtifactCore {

export struct MaterialFractureEvent {
    LayerID layerId;
    int fracturedParticleCount = 0;
    int totalParticleCount = 0;
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

    // --- Phase 2: Fluid Dynamics ---
    /**
     * @brief グローバルな流体シミュレーション（煙・炎等）を初期化する
     */
    void initFluid(int w, int h) {
        fluidSolver_ = std::make_unique<FluidSolver2D>(w, h);
    }
    
    /**
     * @brief 流体ソルバーを取得する
     */
    FluidSolver2D* getFluidSolver() { return fluidSolver_.get(); }
    
    // --- Phase 3: Soft Body Dynamics ---
    /**
     * @brief レイヤー固有のソフトボディソルバーを登録する
     */
    void registerSoftBody(LayerID layerId, std::shared_ptr<SoftBodySolver> solver) {
        softBodies_[layerId] = solver;
        softBodySnapshots_.erase(layerId);
    }

    /**
     * @brief レイヤー用ソフトボディソルバーを生成して登録する
     */
    std::shared_ptr<SoftBodySolver> createSoftBody(LayerID layerId) {
        auto solver = std::make_shared<SoftBodySolver>();
        softBodies_[layerId] = solver;
        softBodySnapshots_.erase(layerId);
        return solver;
    }

    /**
     * @brief レイヤー用ソフトボディを格子で初期化する
     */
    std::shared_ptr<SoftBodySolver> createSoftBodyGrid(
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
    std::shared_ptr<SoftBodySolver> createSoftBodyChain(
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

    std::shared_ptr<MpmSolver2D> createMaterialSolver(
        LayerID layerId, MpmMaterialPreset preset = MpmMaterialPreset::Flesh) {
        auto solver = std::make_shared<MpmSolver2D>();
        solver->applyMaterialPreset(preset);
        materialSolvers_[layerId] = solver;
        materialSnapshots_.erase(layerId);
        return solver;
    }

    std::shared_ptr<MpmSolver2D> createMaterialGrid(
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

    std::shared_ptr<MpmSolver2D> getMaterialSolver(LayerID layerId) {
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
        return std::exchange(pendingMaterialFractureEvents_, {});
    }

    /**
     * @brief レイヤー用 rigid body world を生成して登録する
     */
    std::shared_ptr<Physics2D> createRigidWorld(LayerID layerId) {
        auto world = std::make_shared<Physics2D>();
        rigidWorlds_[layerId] = world;
        return world;
    }

    /**
     * @brief レイヤー用 rigid body world を取得する
     */
    std::shared_ptr<Physics2D> getRigidWorld(LayerID layerId) {
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
    std::shared_ptr<SoftBodySolver> getSoftBody(LayerID layerId) {
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
            return it->second;
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
        if (fluidSolver_) {
            // 流体は密度（熱）による浮力や粘性を考慮して更新
            fluidSolver_->update(dt);
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
            sb->update(dt, gravityX, gravityY);
        }

        for (auto& [id, solver] : materialSolvers_) {
            if (solver) {
                solver->update(dt);
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
                world->step(dt);
            }
        }
    }

    /**
     * @brief シミュレーションを全て破棄する
     */
    void clear() {
        fluidSolver_.reset();
        softBodies_.clear();
        softBodyColliders_.clear();
        softBodySnapshots_.clear();
        materialSolvers_.clear();
        materialSnapshots_.clear();
        pendingMaterialFractureEvents_.clear();
        rigidWorlds_.clear();
    }

private:
    PhysicsSystem() = default;
    ~PhysicsSystem() = default;

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    
    std::unique_ptr<FluidSolver2D> fluidSolver_;
    std::map<LayerID, std::shared_ptr<SoftBodySolver>> softBodies_;
    std::map<LayerID, std::vector<SoftBodyCollider>> softBodyColliders_;
    std::map<LayerID, std::map<int64_t, SoftBodySnapshot>> softBodySnapshots_;
    std::map<LayerID, std::shared_ptr<MpmSolver2D>> materialSolvers_;
    std::map<LayerID, std::map<int64_t, MpmSnapshot2D>> materialSnapshots_;
    std::vector<MaterialFractureEvent> pendingMaterialFractureEvents_;
    std::map<LayerID, std::shared_ptr<Physics2D>> rigidWorlds_;
    static constexpr std::size_t maxSoftBodySnapshotsPerLayer_ = 480;
    static constexpr std::size_t maxMaterialSnapshotsPerLayer_ = 480;
};

} // namespace ArtifactCore
