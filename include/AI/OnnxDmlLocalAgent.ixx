module;
#include <utility>
#include <functional>
#include <memory>
#include <QString>

export module Core.AI.OnnxDmlAgent;

import Core.AI.LocalAgent;
import Core.AI.Context;

export namespace ArtifactCore {

/**
 * @brief ONNX Runtime + DirectML を使う軽量ローカルAIエージェント。
 *
 * 生成系モデルは sentencepiece ベースの .onnx + tokenizer.model 構成を想定します。
 */
class OnnxDmlLocalAgent : public LocalAIAgent {
public:
    OnnxDmlLocalAgent();
    ~OnnxDmlLocalAgent() override;

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
    QString lastError() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
