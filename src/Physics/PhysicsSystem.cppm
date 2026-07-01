module;
#include <utility>
#include <memory>
#include <map>
#include <vector>
#include <QString>
#include <optional>

export module Physics.System;

import Physics.Fluid;
import Physics2D;
import Physics.SoftBody;
import Memory.TrackedPtr;
import Utils.Id;

namespace ArtifactCore {

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
    }

    /**
     * @brief レイヤー用ソフトボディソルバーを生成して登録する
     */
    std::shared_ptr<SoftBodySolver> createSoftBody(LayerID layerId) {
        auto solver = std::make_shared<SoftBodySolver>();
        softBodies_[layerId] = solver;
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
    std::map<LayerID, std::shared_ptr<Physics2D>> rigidWorlds_;
};

} // namespace ArtifactCore
