module;
#include <QString>
#include <QDebug>

module Core.AI.TieredManager;

import std;
import Core.AI.Context;
import Core.AI.LocalAgent;
import Core.AI.CloudAgent;

namespace ArtifactCore {

class TieredAIManager::Impl {
public:
    AIContext globalContext;
    LocalAIAgentPtr localAgent;
    ICloudAIAgentPtr cloudAgent;

    Impl() = default;
};

TieredAIManager::TieredAIManager() : impl_(std::make_unique<Impl>()) {}
TieredAIManager::~TieredAIManager() = default;

TieredAIManager& TieredAIManager::instance() {
    static TieredAIManager inst;
    return inst;
}

void TieredAIManager::setLocalAgent(LocalAIAgentPtr agent) {
    impl_->localAgent = agent;
}

void TieredAIManager::setCloudCredentials(const QString& provider, const QString& apiKey) {
    impl_->cloudAgent = CloudAgentFactory::createByName(provider);
    if (impl_->cloudAgent) {
        impl_->cloudAgent->setApiKey(apiKey);
    }
}

void TieredAIManager::setCloudAgent(ICloudAIAgentPtr agent) {
    impl_->cloudAgent = agent;
}

AIContext& TieredAIManager::globalContext() {
    return impl_->globalContext;
}

void TieredAIManager::processRequest(const QString& prompt, std::function<void(bool, const QString&)> callback) {
    qDebug() << "[TieredAIManager] Received prompt:" << prompt;

    if (impl_->localAgent) {
        bool needsCloud = impl_->localAgent->requiresCloudEscalation(prompt, impl_->globalContext);
        if (!needsCloud) {
            QString result = impl_->localAgent->predictParameter(prompt, impl_->globalContext);
            if (callback) callback(true, result);
            return;
        }
    }

    if (!impl_->cloudAgent || !impl_->cloudAgent->isAvailable()) {
        qWarning() << "[TieredAIManager] Cloud agent not available";
        if (callback) callback(false, "Cloud AI not configured");
        return;
    }

    qDebug() << "[TieredAIManager] Escalating to Cloud AI...";
    auto response = impl_->cloudAgent->chat(
        QString(), prompt, impl_->globalContext);

    if (callback) {
        callback(response.success, response.content);
    }
}

void TieredAIManager::triggerBackgroundAnalysis() {
    if (impl_->localAgent) {
        QString summary = impl_->localAgent->analyzeContext(impl_->globalContext);
        impl_->globalContext.setProjectSummary(summary);
    }
}

ICloudAIAgentPtr TieredAIManager::cloudAgent() const {
    return impl_->cloudAgent;
}

} // namespace ArtifactCore
