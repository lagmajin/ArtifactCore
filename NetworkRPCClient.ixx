module;
#include <memory>
#include <functional>
#include <QString>
#include <QJsonObject>

export module NetworkRPCClient;

export namespace ArtifactCore
{

class NetworkRPCClient
{
public:
    NetworkRPCClient();
    ~NetworkRPCClient();

    NetworkRPCClient(const NetworkRPCClient&) = delete;
    NetworkRPCClient& operator=(const NetworkRPCClient&) = delete;

    bool connectToServer(const QString& host, unsigned short port, const QString& workerId);
    void disconnect();
    bool isConnected() const;
    QString workerId() const;
    void setAuthToken(const QString& token);
    void setCapabilities(const QJsonObject& capabilities);
    void setTlsEnabled(bool enabled, const QString& caCertificateFile = {});

    using JobAssignedCallback = std::function<void(const QJsonObject& jobData)>;
    using DisconnectedCallback = std::function<void()>;

    void setOnJobAssigned(JobAssignedCallback cb);
    void setOnDisconnected(DisconnectedCallback cb);

    bool sendFrameCompleted(int frame);
    bool sendFrameFailed(int frame, const QString& error);
    bool sendWorkerProgress(int completedFrames, int failedFrames, int currentFrame);

private:
    class Impl;
    Impl* impl_;
};

}
