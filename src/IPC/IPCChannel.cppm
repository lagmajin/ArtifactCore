module;
#include <QDataStream>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QHostAddress>
#include <QProcess>
#include <QElapsedTimer>
#include <cstring>

module IPC.IPCChannel;

import IPC.SharedMemoryRingBuffer;

namespace ArtifactCore::IPC {

class IPCChannel::Impl {
public:
    IPCChannelConfig config;
    std::unique_ptr<SharedMemoryRingBuffer> shared;
    std::unique_ptr<QLocalSocket> local;
    std::unique_ptr<QTcpSocket> tcp;
    std::unique_ptr<QProcess> pipe;
    QByteArray receiveBuffer;
    bool streaming = false;

    explicit Impl(const IPCChannelConfig& value) : config(value) {}
    bool waitForSocket(int timeout) {
        if (local) return local->waitForConnected(timeout);
        if (tcp) return tcp->waitForConnected(timeout);
        return false;
    }
    QIODevice* device() {
        if (local) return local.get();
        if (tcp) return tcp.get();
        return pipe.get();
    }
};

IPCChannel::IPCChannel(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
IPCChannel::~IPCChannel() { disconnect(); }

std::unique_ptr<IPCChannel> IPCChannel::create(const IPCChannelConfig& config) {
    if (config.name.trimmed().isEmpty()) return {};
    auto impl = std::make_unique<Impl>(config);
    if (config.transport == IPCTransport::SharedMemory) {
        SharedMemoryRingBuffer::Config ring;
        ring.name = config.name;
        ring.totalSize = std::max<std::size_t>(config.bufferSize * 4, 4096);
        ring.maxEntrySize = config.bufferSize;
        ring.create = config.create;
        impl->shared = config.create ? SharedMemoryRingBuffer::create(ring)
                                     : SharedMemoryRingBuffer::open(config.name);
        if (!impl->shared) {
            IPCChannelConfig fallback = config;
            fallback.transport = IPCTransport::LocalSocket;
            return create(fallback);
        }
    } else if (config.transport == IPCTransport::LocalSocket) {
        impl->local = std::make_unique<QLocalSocket>();
        impl->local->connectToServer(config.name);
        if (!impl->waitForSocket(config.timeoutMs)) return {};
    } else if (config.transport == IPCTransport::TcpSocket) {
        const auto parts = config.name.split(QLatin1Char(':'));
        if (parts.size() != 2) return {};
        bool ok = false;
        const quint16 port = parts.at(1).toUShort(&ok);
        if (!ok) return {};
        impl->tcp = std::make_unique<QTcpSocket>();
        impl->tcp->connectToHost(parts.at(0), port);
        if (!impl->waitForSocket(config.timeoutMs)) return {};
    } else if (config.transport == IPCTransport::Pipe) {
        impl->pipe = std::make_unique<QProcess>();
        impl->pipe->start(config.name);
        if (!impl->pipe->waitForStarted(config.timeoutMs)) return {};
    } else {
        return {};
    }
    return std::unique_ptr<IPCChannel>(new IPCChannel(std::move(impl)));
}

bool IPCChannel::send(const QByteArray& message) {
    if (!impl_) return false;
    if (impl_->shared) return impl_->shared->write(message).success;
    if (!impl_->device()) return false;
    QByteArray framed;
    QDataStream stream(&framed, QIODevice::WriteOnly);
    stream << static_cast<quint32>(message.size());
    framed.append(message);
    const qint64 written = impl_->device()->write(framed);
    const bool flushed = impl_->local ? impl_->local->waitForBytesWritten(impl_->config.timeoutMs)
                         : impl_->tcp ? impl_->tcp->waitForBytesWritten(impl_->config.timeoutMs)
                                      : impl_->pipe->waitForBytesWritten(impl_->config.timeoutMs);
    return written == framed.size() && flushed;
}

QByteArray IPCChannel::receive() {
    if (!impl_) return {};
    if (impl_->shared) {
        const auto result = impl_->shared->read();
        return result.success ? result.data : QByteArray();
    }
    if (!impl_->device()) return {};
    impl_->receiveBuffer.append(impl_->device()->readAll());
    if (impl_->receiveBuffer.size() < 4) return {};
    QDataStream stream(impl_->receiveBuffer);
    quint32 size = 0;
    stream >> size;
    if (size > impl_->config.bufferSize || impl_->receiveBuffer.size() < static_cast<qsizetype>(4 + size)) return {};
    QByteArray result(static_cast<qsizetype>(size), Qt::Uninitialized);
    stream.readRawData(result.data(), static_cast<int>(size));
    impl_->receiveBuffer.remove(0, static_cast<qsizetype>(4 + size));
    return result;
}

QByteArray IPCChannel::receiveBlocking(int timeoutMs) {
    if (!impl_ || impl_->shared) {
        return impl_ && impl_->shared ? impl_->shared->readBlocking(timeoutMs).data : QByteArray();
    }
    const int timeout = timeoutMs < 0 ? impl_->config.timeoutMs : timeoutMs;
    const bool ready = impl_->local ? impl_->local->waitForReadyRead(timeout)
                       : impl_->tcp ? impl_->tcp->waitForReadyRead(timeout)
                                    : impl_->pipe->waitForReadyRead(timeout);
    if (impl_->device()->bytesAvailable() == 0 && !ready) return {};
    return receive();
}

bool IPCChannel::sendZeroCopy(const std::uint8_t* data, std::size_t size) {
    if (impl_ && impl_->shared) return impl_->shared->write(data, size).success;
    return send(QByteArray(reinterpret_cast<const char*>(data), static_cast<qsizetype>(size)));
}

bool IPCChannel::receiveZeroCopy(std::uint8_t* buffer, std::size_t maxSize, std::size_t& received) {
    received = 0;
    const QByteArray data = receive();
    if (data.isEmpty() || data.size() > static_cast<qsizetype>(maxSize)) return false;
    std::memcpy(buffer, data.constData(), static_cast<std::size_t>(data.size()));
    received = static_cast<std::size_t>(data.size());
    return true;
}

bool IPCChannel::beginStream() { if (impl_) impl_->streaming = true; return isConnected(); }
bool IPCChannel::endStream() { if (impl_) impl_->streaming = false; return isConnected(); }
bool IPCChannel::isConnected() const {
    if (!impl_) return false;
    if (impl_->shared) return impl_->shared->isOpen();
    return impl_->local ? impl_->local->state() == QLocalSocket::ConnectedState
         : impl_->tcp ? impl_->tcp->state() == QAbstractSocket::ConnectedState
                      : impl_->pipe && impl_->pipe->state() == QProcess::Running;
}
IPCTransport IPCChannel::transport() const { return impl_ ? impl_->config.transport : IPCTransport::Pipe; }
std::size_t IPCChannel::pendingBytes() const {
    if (!impl_) return 0;
    if (impl_->shared) return static_cast<std::size_t>(impl_->shared->availableForRead());
    return static_cast<std::size_t>(impl_->device() ? impl_->device()->bytesAvailable() : 0);
}
void IPCChannel::disconnect() {
    if (!impl_) return;
    if (impl_->shared) impl_->shared->close();
    if (impl_->local) impl_->local->disconnectFromServer();
    if (impl_->tcp) impl_->tcp->disconnectFromHost();
    if (impl_->pipe) {
        impl_->pipe->closeWriteChannel();
        impl_->pipe->terminate();
    }
}

}
