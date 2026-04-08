module;
#include <utility>
#include <memory>
#include <QString>

export module Core.AI.LlamaAgent;

import Core.AI.LocalAgent;
import Core.AI.Context;

export namespace ArtifactCore {

/**
 * @brief llama.cpp を使用したローカルAIエージェントの実装。
 * GGUF形式のモデルを直接ロードして、分析と会話応答を行います。
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
    LocalAnalysisResult analyzeUserQuestion(const QString& question, const AIContext& context) override;
    QString generateChatResponse(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context) override;
    QString generateChatResponseStreaming(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const std::function<bool(const QString& piece)>& tokenCallback) override;
    QString filterSensitiveInfo(const QString& text) override;

    // llama.cpp 固有のパラメータ設定
    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);
    QString lastError() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    QString generateVisibilityDiagnosis(const QString& collectedData, const AIContext& context);
    QString generateGenericAnswer(const QString& intent, const QString& collectedData);
};

} // namespace ArtifactCore
