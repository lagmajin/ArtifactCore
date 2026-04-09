module;
#include <QString>
#include <QMap>
#include <QSettings>

module Core.AI.APIKeyManager;

import std;
import Core.AI.CloudAgent;

namespace ArtifactCore {

namespace {
QString providerKeyName(CloudProvider provider) {
    switch (provider) {
    case CloudProvider::OpenRouter: return QStringLiteral("ai/openrouter/apikey");
    case CloudProvider::DirectAnthropic: return QStringLiteral("ai/anthropic/apikey");
    case CloudProvider::DirectOpenAI: return QStringLiteral("ai/openai/apikey");
    default: return QStringLiteral("ai/unknown/apikey");
    }
}

QString providerProxyKey(CloudProvider provider) {
    switch (provider) {
    case CloudProvider::OpenRouter: return QStringLiteral("ai/openrouter/proxy");
    case CloudProvider::DirectAnthropic: return QStringLiteral("ai/anthropic/proxy");
    case CloudProvider::DirectOpenAI: return QStringLiteral("ai/openai/proxy");
    default: return QStringLiteral("ai/unknown/proxy");
    }
}
} // namespace

APIKeyManager& APIKeyManager::instance() {
    static APIKeyManager inst;
    return inst;
}

void APIKeyManager::setKey(CloudProvider provider, const QString& key) {
    apiKeys_[provider] = key;
}

QString APIKeyManager::getKey(CloudProvider provider) const {
    return apiKeys_.value(provider);
}

QString APIKeyManager::maskedKey(CloudProvider provider) const {
    const QString key = apiKeys_.value(provider);
    if (key.length() <= 8) {
        return QStringLiteral("***");
    }
    return key.left(4) + QStringLiteral("***") + key.right(4);
}

bool APIKeyManager::hasKey(CloudProvider provider) const {
    return !apiKeys_.value(provider).isEmpty();
}

void APIKeyManager::removeKey(CloudProvider provider) {
    apiKeys_.remove(provider);
}

void APIKeyManager::setProxy(CloudProvider provider, const QString& proxyUrl) {
    proxies_[provider] = proxyUrl;
}

QString APIKeyManager::getProxy(CloudProvider provider) const {
    return proxies_.value(provider);
}

void APIKeyManager::saveToSettings() {
    QSettings settings;
    for (auto it = apiKeys_.constBegin(); it != apiKeys_.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            settings.setValue(providerKeyName(it.key()), it.value());
        }
    }
    for (auto it = proxies_.constBegin(); it != proxies_.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            settings.setValue(providerProxyKey(it.key()), it.value());
        }
    }
    settings.sync();
}

void APIKeyManager::loadFromSettings() {
    QSettings settings;
    for (int i = static_cast<int>(CloudProvider::OpenRouter);
         i <= static_cast<int>(CloudProvider::OpenAI); ++i) {
        auto provider = static_cast<CloudProvider>(i);
        const QString key = settings.value(providerKeyName(provider)).toString();
        if (!key.isEmpty()) {
            apiKeys_[provider] = key;
        }
        const QString proxy = settings.value(providerProxyKey(provider)).toString();
        if (!proxy.isEmpty()) {
            proxies_[provider] = proxy;
        }
    }
}

void APIKeyManager::clearAll() {
    apiKeys_.clear();
    proxies_.clear();
}

QString cloudProviderToString(CloudProvider provider) {
    switch (provider) {
    case CloudProvider::OpenRouter: return QStringLiteral("openrouter");
    case CloudProvider::Anthropic: return QStringLiteral("anthropic");
    case CloudProvider::OpenAI: return QStringLiteral("openai");
    default: return QStringLiteral("unknown");
    }
}

CloudProvider stringToCloudProvider(const QString& str) {
    const QString lower = str.toLower().trimmed();
    if (lower == QStringLiteral("openrouter")) return CloudProvider::OpenRouter;
    if (lower == QStringLiteral("anthropic") || lower == QStringLiteral("directanthropic")) return CloudProvider::DirectAnthropic;
    if (lower == QStringLiteral("openai") || lower == QStringLiteral("directopenai")) return CloudProvider::DirectOpenAI;
    return CloudProvider::Unknown;
}

} // namespace ArtifactCore
