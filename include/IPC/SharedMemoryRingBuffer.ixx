module;
#include <QByteArray>
#include <QString>
#include <QSystemSemaphore>
#include <cstdint>
#include <atomic>
#include <memory>
#include <vector>

export module IPC.SharedMemoryRingBuffer;

class QSystemSemaphore;

export namespace ArtifactCore::IPC {

inline constexpr std::uint32_t kRingBufferMagic = 0x41524246; // ARBF
inline constexpr std::uint32_t kRingBufferVersion = 1;

struct alignas(64) RingBufferHeader {
    alignas(64) std::atomic<std::uint64_t> writeIndex{0};
    alignas(64) std::atomic<std::uint64_t> readIndex{0};
    std::uint64_t totalCapacity = 0;
    std::uint64_t elementSize = 0;
    std::uint32_t magic = kRingBufferMagic;
    std::uint32_t version = kRingBufferVersion;
    char name[64] = {};
};

struct alignas(8) RingBufferEntryHeader {
    std::uint64_t sequenceNumber = 0;
    std::uint64_t timestampNs = 0;
    std::uint32_t payloadSize = 0;
    std::uint32_t flags = 0;
    std::uint32_t checksum = 0;
    std::uint32_t reserved = 0;
};

class SharedMemoryRingBuffer {
public:
    struct Config {
        QString name;
        std::size_t totalSize = 0;
        std::size_t maxEntrySize = 0;
        bool create = true;
        bool persistent = false;
    };

    struct WriteResult {
        bool success = false;
        std::uint64_t sequenceNumber = 0;
        QString error;
    };

    struct ReadResult {
        bool success = false;
        std::uint64_t sequenceNumber = 0;
        std::uint64_t timestampNs = 0;
        QByteArray data;
        std::uint32_t flags = 0;
        QString error;
    };

    struct Stats {
        std::uint64_t totalWrites = 0;
        std::uint64_t totalReads = 0;
        std::uint64_t totalWriteBytes = 0;
        std::uint64_t totalReadBytes = 0;
        std::uint64_t droppedWrites = 0;
        std::uint64_t lastWriteTimestampNs = 0;
        std::uint64_t lastReadTimestampNs = 0;
    };

    static std::unique_ptr<SharedMemoryRingBuffer> create(const Config& config);
    static std::unique_ptr<SharedMemoryRingBuffer> open(const QString& name);
    static bool isAvailable();

    ~SharedMemoryRingBuffer();
    SharedMemoryRingBuffer(const SharedMemoryRingBuffer&) = delete;
    SharedMemoryRingBuffer& operator=(const SharedMemoryRingBuffer&) = delete;

    WriteResult write(const std::uint8_t* data, std::size_t size, std::uint32_t flags = 0);
    WriteResult write(const QByteArray& data, std::uint32_t flags = 0);
    ReadResult read();
    ReadResult readBlocking(int timeoutMs);
    ReadResult readSequence(std::uint64_t sequenceNumber);

    std::uint64_t availableForWrite() const;
    std::uint64_t availableForRead() const;
    bool isEmpty() const;
    bool isFull() const;
    void reset();
    void close();

    QSystemSemaphore* writeSemaphore();
    QSystemSemaphore* readSemaphore();
    Stats stats() const;
    QString name() const;
    std::size_t capacity() const;
    bool isOpen() const;

private:
    explicit SharedMemoryRingBuffer(const Config& config);
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
