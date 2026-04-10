module;
#include <memory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <QEventLoop>
#include <QTimer>

module Core.AI.CloudAgent;

import std;
import Core.AI.Context;

namespace ArtifactCore {

namespace {

QString buildContextSnapshotBlock(const AIContext& context) {
    const QString json = context.toJsonString();
    if (json.trimmed().isEmpty()) {
        return QString();
    }

    return QStringLiteral(
               "\n\n## Live Project Context\n"
               "Use the following snapshot as the current ArtifactStudio state.\n"
               "If the user asks about counts, active selections, background fill, or timeline state, prefer this snapshot over generic guesses.\n"
               "```json\n%1\n```\n")
        .arg(json);
}

QJsonObject createOpenRouterRequest(
    const QString& model,
    const QString& systemPrompt,
    const QString& userPrompt,
    const QList<ChatMessage>& messages,
    int maxTokens) {

    QJsonArray contents;

    if (!systemPrompt.isEmpty()) {
        contents.push_back(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                       {QStringLiteral("content"), systemPrompt}});
    }

    for (const auto& msg : messages) {
        contents.push_back(QJsonObject{{QStringLiteral("role"), msg.role},
                                       {QStringLiteral("content"), msg.content}});
    }

    if (!userPrompt.isEmpty()) {
        contents.push_back(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                       {QStringLiteral("content"), userPrompt}});
    }

    QJsonObject request;
    request[QStringLiteral("model")] = model;
    request[QStringLiteral("messages")] = contents;
    request[QStringLiteral("max_tokens")] = maxTokens > 0 ? maxTokens : 4096;
    request[QStringLiteral("stream")] = false;

    return request;
}

ChatResponse parseOpenRouterResponse(const QByteArray& data) {
    ChatResponse response;

    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        response.success = false;
        response.errorMessage = error.errorString();
        return response;
    }

    const auto obj = doc.object();
    if (obj.contains(QStringLiteral("error"))) {
        response.success = false;
        const auto err = obj[QStringLiteral("error")];
        if (err.isObject()) {
            response.errorMessage = err[QStringLiteral("message")].toString();
        } else {
            response.errorMessage = err.toString();
        }
        return response;
    }

    const auto choices = obj[QStringLiteral("choices")].toArray();
    if (choices.isEmpty()) {
        response.success = false;
        response.errorMessage = QStringLiteral("No choices in response");
        return response;
    }

    const auto first = choices.first().toObject();
    const auto message = first[QStringLiteral("message")].toObject();
    response.content = message[QStringLiteral("content")].toString();
    response.model = obj[QStringLiteral("model")].toString();

    const auto usage = obj[QStringLiteral("usage")].toObject();
    response.inputTokens = usage[QStringLiteral("prompt_tokens")].toInt();
    response.outputTokens = usage[QStringLiteral("completion_tokens")].toInt();

    response.success = true;
    return response;
}

int estimateTokenCount(const QString& text) {
    return text.length() / 4;
}

CloudProvider providerFromName(const QString& providerName) {
    const QString lower = providerName.trimmed().toLower();
    if (lower == QStringLiteral("openrouter")) {
        return CloudProvider::OpenRouter;
    }
    if (lower == QStringLiteral("anthropic") || lower == QStringLiteral("directanthropic")) {
        return CloudProvider::DirectAnthropic;
    }
    if (lower == QStringLiteral("openai") || lower == QStringLiteral("directopenai")) {
        return CloudProvider::DirectOpenAI;
    }
    return CloudProvider::Unknown;
}

} // namespace

class OpenRouterAgent : public ICloudAIAgent {
public:
    OpenRouterAgent() = default;

    CloudProvider provider() const override { return CloudProvider::OpenRouter; }
    QString providerName() const override { return QStringLiteral("openrouter"); }
    QString defaultModel() const override { return QStringLiteral("anthropic/claude-3.5-sonnet"); }

    bool isAvailable() const override {
        return !apiKey_.isEmpty();
    }

    QString lastError() const override { return lastError_; }

    void setApiKey(const QString& apiKey) override { apiKey_ = apiKey; }
    QString maskedApiKey() const override {
        if (apiKey_.length() <= 8) return QStringLiteral("***");
        return apiKey_.left(4) + QStringLiteral("***") + apiKey_.right(4);
    }

    void setProxy(const QString& proxyUrl) override { proxyUrl_ = proxyUrl; }
    QString proxy() const override { return proxyUrl_; }

    bool initialize() override {
        return isAvailable();
    }

    void shutdown() override {
        apiKey_.clear();
    }

    ChatResponse chat(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const QString& model) override {
        if (!isAvailable()) {
            return {QString(), QString(), 0, 0, false, QStringLiteral("API key not set")};
        }

        const QString actualModel = model.isEmpty() ? defaultModel() : model;
        const QString augmentedSystemPrompt =
            systemPrompt + buildContextSnapshotBlock(context);
        const auto requestJson = createOpenRouterRequest(
            actualModel, augmentedSystemPrompt, userPrompt, {}, 4096);

        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl(QStringLiteral("https://openrouter.ai/api/v1/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey_).toUtf8());
        if (!proxyUrl_.isEmpty()) {
            const QUrl proxyUrl(proxyUrl_);
            if (proxyUrl.isValid() && !proxyUrl.host().isEmpty()) {
                QNetworkProxy proxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyUrl.port(8080));
                if (!proxyUrl.userName().isEmpty()) {
                    proxy.setUser(proxyUrl.userName());
                }
                if (!proxyUrl.password().isEmpty()) {
                    proxy.setPassword(proxyUrl.password());
                }
                manager.setProxy(proxy);
            }
        }

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(60000);

        QNetworkReply* reply = nullptr;
        QObject::connect(&timeout, &QTimer::timeout, [&]() {
            if (reply && !reply->isFinished()) {
                reply->abort();
            }
        });

        reply = manager.post(request, QJsonDocument(requestJson).toJson());
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        timeout.start();
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            lastError_ = reply->errorString();
            return {QString(), QString(), 0, 0, false, lastError_};
        }

        const auto response = parseOpenRouterResponse(reply->readAll());
        if (!response.success) {
            lastError_ = response.errorMessage;
        }
        return response;
    }

    ChatResponse chatStream(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const std::function<bool(const QString&)>& tokenCallback,
        const QString& model) override {
        Q_UNUSED(tokenCallback);

        return chat(systemPrompt, userPrompt, context, model);
    }

    QStringList availableModels() const override {
        return QStringList{
            QStringLiteral("anthropic/claude-3-haiku"),
            QStringLiteral("anthropic/claude-3-sonnet"),
            QStringLiteral("anthropic/claude-3.5-sonnet"),
            QStringLiteral("openai/gpt-4o"),
            QStringLiteral("openai/gpt-4o-mini"),
            QStringLiteral("google/gemini-pro-1.5"),
        };
    }

    int maxContextTokens() const override { return 200000; }
    int estimateTokens(const QString& text) const override { return estimateTokenCount(text); }

    ChatResponse chatWithMessages(
        const QString& systemPrompt,
        const QList<ChatMessage>& messages,
        const QString& model) override {

        if (!isAvailable()) {
            return {QString(), QString(), 0, 0, false, QStringLiteral("API key not set")};
        }

        const QString actualModel = model.isEmpty() ? defaultModel() : model;
        const auto requestJson = createOpenRouterRequest(
            actualModel, systemPrompt, QString(), messages, 4096);

        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl(QStringLiteral("https://openrouter.ai/api/v1/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey_).toUtf8());

        QEventLoop loop;
        QNetworkReply* reply = manager.post(request, QJsonDocument(requestJson).toJson());
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            lastError_ = reply->errorString();
            return {QString(), QString(), 0, 0, false, lastError_};
        }

        const auto response = parseOpenRouterResponse(reply->readAll());
        if (!response.success) {
            lastError_ = response.errorMessage;
        }
        return response;
    }

private:
    QString apiKey_;
    QString proxyUrl_;
    QString lastError_;
};

ICloudAIAgentPtr CloudAgentFactory::create(CloudProvider provider) {
    switch (provider) {
    case CloudProvider::OpenRouter:
        return std::make_shared<OpenRouterAgent>();
    case CloudProvider::DirectAnthropic:
    case CloudProvider::DirectOpenAI:
    default:
        return nullptr;
    }
}

ICloudAIAgentPtr CloudAgentFactory::createByName(const QString& providerName) {
    return create(providerFromName(providerName));
}

QStringList CloudAgentFactory::supportedProviderNames() {
    return QStringList{
        QStringLiteral("openrouter"),
        QStringLiteral("anthropic"),
        QStringLiteral("openai"),
    };
}

} // namespace ArtifactCore
