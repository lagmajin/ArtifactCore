module;
#include <utility>
#include <memory>
#include <functional>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QtNetwork/QTcpServer>

export module NetworkRPCServer;

export namespace ArtifactCore
{

struct RemoteWorkerInfo {
    QString workerId;
    QString address;
    int port = 0;
    qint64 lastHeartbeat = 0;
    bool connected = false;
    int assignedFrames = 0;
};

using WorkerConnectedCallback = std::function<void(const RemoteWorkerInfo&)>;
using WorkerDisconnectedCallback = std::function<void(const QString& workerId)>;
using WorkerHeartbeatCallback = std::function<void(const QString& workerId)>;
using RpcRequestHandler = std::function<QJsonObject(const QString& method, const QJsonObject& params)>;

class NetworkPCServer
{
public:
    NetworkPCServer();
    ~NetworkPCServer();

    NetworkPCServer(const NetworkPCServer&) = delete;
    NetworkPCServer& operator=(const NetworkPCServer&) = delete;

    bool start(unsigned short port);
    void stop();
    bool isRunning() const;
    unsigned short port() const;

    QString call(const QString& function, const QStringList& args);

    // Farm-specific RPC
    QString callWorker(const QString& workerId, const QString& method, const QJsonObject& params);
    bool sendJobAssignment(const QString& workerId, const QJsonObject& jobJson);
    QJsonObject requestWorkerStatus(const QString& workerId);

    // Callbacks
    void setOnWorkerConnected(WorkerConnectedCallback cb);
    void setOnWorkerDisconnected(WorkerDisconnectedCallback cb);
    void setOnWorkerHeartbeat(WorkerHeartbeatCallback cb);
    void setOnRequest(RpcRequestHandler handler);

    // Worker management
    std::vector<RemoteWorkerInfo> connectedWorkers() const;
    RemoteWorkerInfo workerInfo(const QString& workerId) const;
    bool hasWorker(const QString& workerId) const;
    int activeWorkerCount() const;

    static NetworkPCServer& instance();

private:
    class Impl;
    Impl* impl_;
};

}
