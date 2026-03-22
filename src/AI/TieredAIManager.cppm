module;
#include <QString>
#include <QDebug>
#include <memory>
#include <functional>

module Core.AI.TieredManager;

import std;
import Core.AI.Context;
import Core.AI.LocalAgent;

namespace ArtifactCore {

class TieredAIManager::Impl {
public:
    AIContext globalContext;
    LocalAIAgentPtr localAgent;
    QString cloudProvider;
    QString cloudApiKey;

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
    impl_->cloudProvider = provider;
    impl_->cloudApiKey = apiKey;
}

AIContext& TieredAIManager::globalContext() {
    return impl_->globalContext;
}

void TieredAIManager::processRequest(const QString& prompt, std::function<void(bool, const QString&)> callback) {
    qDebug() << "[TieredAIManager] Received prompt:" << prompt;

    // 1. ローカルAIが設定されていれば、まずローカルで解決できるか試みる
    if (impl_->localAgent) {
        bool needsCloud = impl_->localAgent->requiresCloudEscalation(prompt, impl_->globalContext);
        if (!needsCloud) {
            // ローカルで解決
            QString result = impl_->localAgent->predictParameter(prompt, impl_->globalContext);
            if (callback) callback(true, result);
            return;
        }
    }

    // 2. クラウドAIへのエスカレーション（今後の実装）
    if (impl_->cloudApiKey.isEmpty()) {
        qWarning() << "[TieredAIManager] Cloud API key not set. Cannot escalate.";
        if (callback) callback(false, "Cloud API key missing");
        return;
    }

    qDebug() << "[TieredAIManager] Escalating to Cloud AI (" << impl_->cloudProvider << ")...";
    // ここで QNetworkAccessManager 等を使ってクラウドAPIを叩き、
    // impl_->globalContext.toJsonString() と prompt を送信する実装が入る。
    
    // シミュレーション
    if (callback) {
        callback(true, "Cloud AI response (simulated) based on: " + prompt);
    }
}

void TieredAIManager::triggerBackgroundAnalysis() {
    if (impl_->localAgent) {
        // 現在のコンテキストを要約・前処理させておく（キャッシュ化）
        QString summary = impl_->localAgent->analyzeContext(impl_->globalContext);
        impl_->globalContext.setProjectSummary(summary);
    }
}

} // namespace ArtifactCore
