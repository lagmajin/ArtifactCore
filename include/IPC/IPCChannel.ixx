module;
#include <QByteArray>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>

export module IPC.IPCChannel;

export namespace ArtifactCore::IPC {

enum class IPCTransport { SharedMemory, LocalSocket, TcpSocket, Pipe };

struct IPCChannelConfig {
    IPCTransport transport = IPCTransport::SharedMemory;
    QString name;
    std::size_t bufferSize = 65536;
    int timeoutMs = 5000;
    bool encrypted = false;
    bool create = false;
};

class IPCChannel {
public:
    static std::unique_ptr<IPCChannel> create(const IPCChannelConfig& config);
    ~IPCChannel();

    bool send(const QByteArray& message);
    QByteArray receive();
    QByteArray receiveBlocking(int timeoutMs = -1);
    bool sendZeroCopy(const std::uint8_t* data, std::size_t size);
    bool receiveZeroCopy(std::uint8_t* buffer, std::size_t maxSize, std::size_t& received);
    bool beginStream();
    bool endStream();
    bool isConnected() const;
    IPCTransport transport() const;
    std::size_t pendingBytes() const;
    void disconnect();

private:
    class Impl;
    explicit IPCChannel(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}
