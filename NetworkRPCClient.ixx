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

    using JobAssignedCallback = std::function<void(const QJsonObject& jobData)>;
    using DisconnectedCallback = std::function<void()>;

    void setOnJobAssigned(JobAssignedCallback cb);
    void setOnDisconnected(DisconnectedCallback cb);

    bool sendFrameCompleted(int frame);
    bool sendFrameFailed(int frame, const QString& error);

private:
    class Impl;
    Impl* impl_;
};

}
