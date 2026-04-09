module;
#include <utility>
#include <memory>
#include <functional>
#include <QString>
#include <QMap>
#include <QList>
#include <QStringList>
#include <QVariant>

export module Core.AI.CloudAgent;

import Core.AI.Context;

export namespace ArtifactCore {

enum class CloudProvider {
    OpenRouter,
    DirectAnthropic,
    DirectOpenAI,
    Unknown
};

struct CloudMessage {
    QString role;
    QString content;
};

struct CloudResponse {
    QString content;
    QString model;
    int tokensUsed = 0;
    bool success = false;
    QString errorMessage;
};

using ChatMessage = CloudMessage;

struct ChatResponse {
    QString content;
    QString model;
    int inputTokens = 0;
    int outputTokens = 0;
    bool success = false;
    QString errorMessage;
};

class ICloudAIAgent {
public:
    virtual ~ICloudAIAgent() = default;

    virtual CloudProvider provider() const = 0;
    virtual QString providerName() const = 0;
    virtual QString defaultModel() const = 0;

    virtual bool isAvailable() const = 0;
    virtual QString lastError() const = 0;

    virtual void setApiKey(const QString& apiKey) = 0;
    virtual QString maskedApiKey() const = 0;

    virtual void setProxy(const QString& proxyUrl) = 0;
    virtual QString proxy() const = 0;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual ChatResponse chat(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const QString& model = {}) = 0;

    virtual ChatResponse chatStream(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const std::function<bool(const QString& piece)>& tokenCallback,
        const QString& model = {}) = 0;

    virtual QStringList availableModels() const = 0;

    virtual int maxContextTokens() const = 0;
    virtual int estimateTokens(const QString& text) const = 0;

    virtual ChatResponse chatWithMessages(
        const QString& systemPrompt,
        const QList<ChatMessage>& messages,
        const QString& model = {}) = 0;
};

using ICloudAIAgentPtr = std::shared_ptr<ICloudAIAgent>;

class CloudAgentFactory {
public:
    static ICloudAIAgentPtr create(CloudProvider provider);
    static ICloudAIAgentPtr createByName(const QString& providerName);
    static QStringList supportedProviderNames();
};

} // namespace ArtifactCore
