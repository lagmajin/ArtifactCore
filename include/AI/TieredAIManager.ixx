module;
#include <utility>
#include <memory>
#include <functional>
#include <QString>

export module Core.AI.TieredManager;

import Core.AI.Context;
import Core.AI.LocalAgent;
import Core.AI.CloudAgent;

export namespace ArtifactCore {

class TieredAIManager {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    TieredAIManager();

public:
    ~TieredAIManager();

    static TieredAIManager& instance();

    void setLocalAgent(LocalAIAgentPtr agent);
    void setCloudAgent(ICloudAIAgentPtr agent);
    ICloudAIAgentPtr cloudAgent() const;

    void setCloudCredentials(const QString& provider, const QString& apiKey);

    AIContext& globalContext();

    void processRequest(const QString& prompt, std::function<void(bool success, const QString& result)> callback);

    void triggerBackgroundAnalysis();
};

} // namespace ArtifactCore
