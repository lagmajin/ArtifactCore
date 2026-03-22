module;
#include <QString>
#include <memory>

export module Core.AI.LlamaAgent;

import Core.AI.LocalAgent;
import Core.AI.Context;

export namespace ArtifactCore {

/**
 * @brief llama.cpp を使用したローカルAIエージェントの実装。
 * GGUF形式のモデルをロードし、テキスト生成や分析を行います。
 */
class LlamaLocalAgent : public LocalAIAgent {
public:
    LlamaLocalAgent();
    virtual ~LlamaLocalAgent();

    // LocalAIAgent interface implementation
    bool initialize(const QString& modelPath) override;
    QString analyzeContext(const AIContext& context) override;
    QString predictParameter(const QString& targetProperty, const AIContext& context) override;
    bool requiresCloudEscalation(const QString& userPrompt, const AIContext& context) override;

    // llama.cpp 固有のパラメータ設定
    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
