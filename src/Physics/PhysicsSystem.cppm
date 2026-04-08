module;
#include <utility>
#include <memory>
#include <map>
#include <QString>
#include <optional>

export module Physics.System;

import Physics.Fluid;
import Physics.SoftBody;
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
     * @brief 指定したレイヤーのソフトボディソルバーを取得する
     */
    std::shared_ptr<SoftBodySolver> getSoftBody(LayerID layerId) {
        auto it = softBodies_.find(layerId);
        if (it != softBodies_.end()) return it->second;
        return nullptr;
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
            sb->update(dt, gravityX, gravityY);
        }
    }

    /**
     * @brief シミュレーションを全て破棄する
     */
    void clear() {
        fluidSolver_.reset();
        softBodies_.clear();
    }

private:
    PhysicsSystem() = default;
    ~PhysicsSystem() = default;

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    
    std::unique_ptr<FluidSolver2D> fluidSolver_;
    std::map<LayerID, std::shared_ptr<SoftBodySolver>> softBodies_;
};

} // namespace ArtifactCore
