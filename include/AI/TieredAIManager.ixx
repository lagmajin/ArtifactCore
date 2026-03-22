module;
#include <QString>
#include <memory>
#include <functional>

export module Core.AI.TieredManager;

import Core.AI.Context;
import Core.AI.LocalAgent;

export namespace ArtifactCore {

/**
 * @brief ローカルAIとクラウドAIを統合管理するマネージャー。
 * Copilotからのリクエストを受け取り、ローカルで処理するかクラウドに投げるかを判断する。
 */
class TieredAIManager {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    TieredAIManager();

public:
    ~TieredAIManager();

    static TieredAIManager& instance();

    /**
     * @brief ローカルエージェントの登録
     */
    void setLocalAgent(LocalAIAgentPtr agent);

    /**
     * @brief クラウドのAPIキー等の設定
     */
    void setCloudCredentials(const QString& provider, const QString& apiKey);

    /**
     * @brief 現在のグローバルコンテキストを取得
     */
    AIContext& globalContext();

    /**
     * @brief ユーザーからのリクエストを処理する
     * @param prompt ユーザーの入力（例："レイヤーをキラキラさせて"）
     * @param callback 処理完了時のコールバック（生成されたスクリプトやパラメータを返す）
     */
    void processRequest(const QString& prompt, std::function<void(bool success, const QString& result)> callback);

    /**
     * @brief バックグラウンドでコンテキストを要約・前処理させる
     */
    void triggerBackgroundAnalysis();
};

} // namespace ArtifactCore
