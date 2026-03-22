module;
#include <QString>
#include <memory>
#include <functional>

export module Core.AI.LocalAgent;

import Core.AI.Context;

export namespace ArtifactCore {

/**
 * @brief ローカルで動作する軽量AI（Tier 1）のインターフェース。
 * ONNX Runtimeや小規模なローカルLLMをバックエンドに持つことを想定。
 */
class LocalAIAgent {
public:
    virtual ~LocalAIAgent() = default;

    /**
     * @brief AIモデルの初期化（モデルのロードなど）
     */
    virtual bool initialize(const QString& modelPath) = 0;

    /**
     * @brief 現在のコンテキストを分析し、ユーザーの意図を推論する
     * @param context 収集されたアプリケーションのコンテキスト
     * @return 推論結果（意図のサマリーや、クラウドへのエスカレーションが必要かどうかのフラグ）
     */
    virtual QString analyzeContext(const AIContext& context) = 0;

    /**
     * @brief 特定のプロパティ値の自動調整など、軽量なタスクを即座に実行する
     * @return 成功した場合はJSON形式のパラメータなどを返す
     */
    virtual QString predictParameter(const QString& targetProperty, const AIContext& context) = 0;

    /**
     * @brief クラウドAI（Tier 2）の助けが必要かどうかを判断する
     */
    virtual bool requiresCloudEscalation(const QString& userPrompt, const AIContext& context) = 0;
};

using LocalAIAgentPtr = std::shared_ptr<LocalAIAgent>;

} // namespace ArtifactCore
