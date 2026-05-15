module;
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative.Manager;

import Graphics.Effect.Creative;
import Channel;

export namespace ArtifactCore {

/**
 * @brief エフェクトの重なり（スタック）を管理するクラス
 * レイヤーごとに複数のクリエイティブエフェクトを保持し、順番に適用します。
 */
class LIBRARY_DLL_API CreativeEffectManager {
public:
    CreativeEffectManager();
    ~CreativeEffectManager();

    /**
     * @brief エフェクトをスタックに追加
     */
    void addEffect(std::shared_ptr<CreativeEffect> effect);
    
    /**
     * @brief 指定したインデックスのエフェクトを削除
     */
    void removeEffect(int index);

    /**
     * @brief 全てのエフェクトを順番に適用
     * @param frame 入出力、各エフェクトが破壊的に変更を加えていきます
     * @param context 共通のコンテキスト情報
     */
    void applyAll(VideoFrame& frame, const CreativeEffectContext& context);

    /**
     * @brief エフェクト群を取得
     */
    const std::vector<std::shared_ptr<CreativeEffect>>& getEffects() const { return effectStack_; }

private:
    std::vector<std::shared_ptr<CreativeEffect>> effectStack_;
};

} // namespace ArtifactCore
