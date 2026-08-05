module;
#include <QString>
#include <QMap>
#include <QDir>
#include <QStandardPaths>

module Core.AI.APIKeyManager;

import std;
import Core.AI.CloudAgent;
import Configuration.ConfigLayer;
import Configuration.LayeredConfigStore;

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

LayeredConfigStore& settingsStore() {
    auto& store = LayeredConfigStore::instance();
    if (!store.isLoaded(ConfigLayer::User)) {
        const QString path = QDir(QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation)).filePath(QStringLiteral("settings.cbor"));
        store.loadLayer(ConfigLayer::User, path);
    }
    return store;
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
    auto& settings = settingsStore();
    for (auto it = apiKeys_.constBegin(); it != apiKeys_.constEnd(); ++it) {
        const QString key = providerKeyName(it.key());
        if (it.value().isEmpty()) settings.removeValue(ConfigLayer::User, key.toStdString());
        else settings.setValue(ConfigLayer::User, key.toStdString(), it.value());
    }
    for (auto it = proxies_.constBegin(); it != proxies_.constEnd(); ++it) {
        const QString key = providerProxyKey(it.key());
        if (it.value().isEmpty()) settings.removeValue(ConfigLayer::User, key.toStdString());
        else settings.setValue(ConfigLayer::User, key.toStdString(), it.value());
    }
    settings.saveLayer(ConfigLayer::User);
}

void APIKeyManager::loadFromSettings() {
    auto& settings = settingsStore();
    for (int i = static_cast<int>(CloudProvider::OpenRouter);
         i <= static_cast<int>(CloudProvider::DirectOpenAI); ++i) {
        auto provider = static_cast<CloudProvider>(i);
        const QString key = settings.valueString(providerKeyName(provider).toStdString());
        if (!key.isEmpty()) {
            apiKeys_[provider] = key;
        }
        const QString proxy = settings.valueString(providerProxyKey(provider).toStdString());
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
    case CloudProvider::DirectAnthropic: return QStringLiteral("anthropic");
    case CloudProvider::DirectOpenAI: return QStringLiteral("openai");
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
