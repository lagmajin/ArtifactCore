module;
#include <QElapsedTimer>
#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QThread>
#include <QDateTime>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>

module IPC.SharedMemoryRingBuffer;

import std;

namespace ArtifactCore::IPC {

namespace {
constexpr std::uint32_t kWrapMarker = std::numeric_limits<std::uint32_t>::max();

std::size_t align8(std::size_t value) { return (value + 7u) & ~std::size_t(7u); }

std::uint64_t monotonicNs() {
    static const auto origin = std::chrono::steady_clock::now();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - origin).count());
}

std::uint32_t crc32c(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1u) ^ (0x82f63b78u & mask);
        }
    }
    return ~crc;
}

QString semaphoreName(const QString& name, const char* suffix) {
    return QStringLiteral("%1_%2").arg(name, QString::fromLatin1(suffix));
}
}

class SharedMemoryRingBuffer::Impl {
public:
    Config config;
    QSharedMemory memory;
    RingBufferHeader* header = nullptr;
    std::uint8_t* data = nullptr;
    std::unique_ptr<QSystemSemaphore> writeReady;
    std::unique_ptr<QSystemSemaphore> readComplete;
    mutable std::mutex localMutex;
    std::atomic<std::uint64_t> totalWrites{0};
    std::atomic<std::uint64_t> totalReads{0};
    std::atomic<std::uint64_t> totalWriteBytes{0};
    std::atomic<std::uint64_t> totalReadBytes{0};
    std::atomic<std::uint64_t> droppedWrites{0};
    std::atomic<std::uint64_t> lastWriteTimestampNs{0};
    std::atomic<std::uint64_t> lastReadTimestampNs{0};
    bool attached = false;

    explicit Impl(const Config& value) : config(value), memory(value.name) {}

    bool bindExisting() {
        if (!memory.isAttached()) return false;
        if (memory.size() < static_cast<int>(sizeof(RingBufferHeader))) return false;
        header = static_cast<RingBufferHeader*>(memory.data());
        if (header->magic != kRingBufferMagic || header->version != kRingBufferVersion ||
            header->totalCapacity == 0 ||
            header->totalCapacity > static_cast<std::uint64_t>(memory.size() - sizeof(RingBufferHeader))) {
            return false;
        }
        data = reinterpret_cast<std::uint8_t*>(header) + sizeof(RingBufferHeader);
        config.name = QString::fromUtf8(header->name);
        config.totalSize = sizeof(RingBufferHeader) + static_cast<std::size_t>(header->totalCapacity);
        config.maxEntrySize = config.maxEntrySize == 0
            ? static_cast<std::size_t>(header->totalCapacity - sizeof(RingBufferEntryHeader))
            : config.maxEntrySize;
        writeReady = std::make_unique<QSystemSemaphore>(semaphoreName(config.name, "write"), 0,
                                                        QSystemSemaphore::Open);
        readComplete = std::make_unique<QSystemSemaphore>(semaphoreName(config.name, "read"), 0,
                                                          QSystemSemaphore::Open);
        attached = true;
        return true;
    }

    void detach() {
        std::scoped_lock lock(localMutex);
        if (memory.isAttached()) memory.detach();
        header = nullptr;
        data = nullptr;
        attached = false;
    }
};

SharedMemoryRingBuffer::SharedMemoryRingBuffer(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

SharedMemoryRingBuffer::~SharedMemoryRingBuffer() { close(); }

std::unique_ptr<SharedMemoryRingBuffer> SharedMemoryRingBuffer::create(const Config& config) {
    if (config.name.trimmed().isEmpty() || config.totalSize <= sizeof(RingBufferHeader) + sizeof(RingBufferEntryHeader)) return {};
    const std::size_t capacity = config.totalSize - sizeof(RingBufferHeader);
    const std::size_t maxEntry = config.maxEntrySize == 0 ? capacity - sizeof(RingBufferEntryHeader) : config.maxEntrySize;
    if (maxEntry == 0 || maxEntry + sizeof(RingBufferEntryHeader) > capacity) return {};
    auto result = std::unique_ptr<SharedMemoryRingBuffer>(new SharedMemoryRingBuffer(config));
    if (!result->impl_->memory.create(static_cast<int>(config.totalSize))) return {};
    std::memset(result->impl_->memory.data(), 0, static_cast<std::size_t>(result->impl_->memory.size()));
    auto* header = static_cast<RingBufferHeader*>(result->impl_->memory.data());
    header->totalCapacity = capacity;
    header->elementSize = 0;
    header->magic = kRingBufferMagic;
    header->version = kRingBufferVersion;
    const QByteArray name = config.name.toUtf8();
    std::memcpy(header->name, name.constData(),
                std::min<std::size_t>(name.size(), sizeof(header->name) - 1));
    if (!result->impl_->bindExisting()) return {};
    result->impl_->config.maxEntrySize = maxEntry;
    result->impl_->writeReady = std::make_unique<QSystemSemaphore>(semaphoreName(config.name, "write"), 0,
                                                                    QSystemSemaphore::Create);
    result->impl_->readComplete = std::make_unique<QSystemSemaphore>(semaphoreName(config.name, "read"), 0,
                                                                      QSystemSemaphore::Create);
    return result;
}

std::unique_ptr<SharedMemoryRingBuffer> SharedMemoryRingBuffer::open(const QString& name) {
    if (name.trimmed().isEmpty()) return {};
    Config config;
    config.name = name;
    config.create = false;
    auto result = std::unique_ptr<SharedMemoryRingBuffer>(new SharedMemoryRingBuffer(config));
    if (!result->impl_->memory.attach(QSharedMemory::ReadWrite) || !result->impl_->bindExisting()) return {};
    return result;
}

bool SharedMemoryRingBuffer::isAvailable() {
    // QSharedMemory is provided by QtCore on all supported platforms. A
    // concrete segment is deliberately not created by this capability probe.
    return true;
}

SharedMemoryRingBuffer::WriteResult SharedMemoryRingBuffer::write(const std::uint8_t* bytes,
                                                                   std::size_t size,
                                                                   std::uint32_t flags) {
    if (!impl_->attached || !bytes || size == 0) return {false, 0, QStringLiteral("Invalid buffer or payload")};
    if (size > impl_->config.maxEntrySize) return {false, 0, QStringLiteral("Payload exceeds maxEntrySize")};
    const std::size_t rawSize = sizeof(RingBufferEntryHeader) + size;
    const std::size_t entrySize = align8(rawSize);
    const std::uint64_t capacity = impl_->header->totalCapacity;
    std::scoped_lock lock(impl_->localMutex);
    const std::uint64_t write = impl_->header->writeIndex.load(std::memory_order_relaxed);
    const std::uint64_t read = impl_->header->readIndex.load(std::memory_order_acquire);
    const std::uint64_t used = write - read;
    const std::uint64_t offset = write % capacity;
    const std::uint64_t tail = capacity - offset;
    const std::uint64_t required = entrySize <= tail ? entrySize : tail + entrySize;
    if (required > capacity - std::min(used, capacity)) {
        impl_->droppedWrites.fetch_add(1, std::memory_order_relaxed);
        return {false, 0, QStringLiteral("Buffer full")};
    }
    std::uint64_t actualWrite = write;
    if (entrySize > tail) {
        if (tail >= sizeof(RingBufferEntryHeader)) {
            RingBufferEntryHeader marker{};
            marker.sequenceNumber = write;
            marker.payloadSize = kWrapMarker;
            std::memcpy(impl_->data + offset, &marker, sizeof(marker));
        }
        actualWrite += tail;
    }
    const std::uint64_t writeOffset = actualWrite % capacity;
    RingBufferEntryHeader entry{};
    entry.sequenceNumber = actualWrite;
    entry.timestampNs = monotonicNs();
    entry.payloadSize = static_cast<std::uint32_t>(size);
    entry.flags = flags;
    entry.checksum = crc32c(bytes, size);
    std::memcpy(impl_->data + writeOffset, &entry, sizeof(entry));
    std::memcpy(impl_->data + writeOffset + sizeof(entry), bytes, size);
    if (entrySize > rawSize) std::memset(impl_->data + writeOffset + rawSize, 0, entrySize - rawSize);
    impl_->header->writeIndex.store(actualWrite + entrySize, std::memory_order_release);
    impl_->totalWrites.fetch_add(1, std::memory_order_relaxed);
    impl_->totalWriteBytes.fetch_add(size, std::memory_order_relaxed);
    impl_->lastWriteTimestampNs.store(entry.timestampNs, std::memory_order_relaxed);
    if (impl_->writeReady) impl_->writeReady->release();
    return {true, entry.sequenceNumber, {}};
}

SharedMemoryRingBuffer::WriteResult SharedMemoryRingBuffer::write(const QByteArray& data, std::uint32_t flags) {
    return write(reinterpret_cast<const std::uint8_t*>(data.constData()), static_cast<std::size_t>(data.size()), flags);
}

SharedMemoryRingBuffer::ReadResult SharedMemoryRingBuffer::read() {
    if (!impl_->attached) return {false, 0, 0, {}, 0, QStringLiteral("Buffer is closed")};
    const std::uint64_t capacity = impl_->header->totalCapacity;
    std::scoped_lock lock(impl_->localMutex);
    for (;;) {
        const std::uint64_t read = impl_->header->readIndex.load(std::memory_order_relaxed);
        const std::uint64_t write = impl_->header->writeIndex.load(std::memory_order_acquire);
        if (read == write) return {false, 0, 0, {}, 0, QStringLiteral("Buffer is empty")};
        const std::uint64_t offset = read % capacity;
        const std::uint64_t tail = capacity - offset;
        if (tail < sizeof(RingBufferEntryHeader)) {
            impl_->header->readIndex.store(read + tail, std::memory_order_release);
            continue;
        }
        RingBufferEntryHeader entry{};
        std::memcpy(&entry, impl_->data + offset, sizeof(entry));
        if (entry.payloadSize == kWrapMarker) {
            impl_->header->readIndex.store(read + tail, std::memory_order_release);
            continue;
        }
        const std::size_t payloadSize = entry.payloadSize;
        const std::size_t entrySize = align8(sizeof(entry) + payloadSize);
        if (payloadSize > impl_->config.maxEntrySize || entrySize > tail || read + entrySize > write) {
            impl_->header->readIndex.store(write, std::memory_order_release);
            return {false, 0, 0, {}, 0, QStringLiteral("Corrupt ring buffer entry")};
        }
        QByteArray payload(reinterpret_cast<const char*>(impl_->data + offset + sizeof(entry)),
                           static_cast<qsizetype>(payloadSize));
        impl_->header->readIndex.store(read + entrySize, std::memory_order_release);
        if (crc32c(reinterpret_cast<const std::uint8_t*>(payload.constData()), payloadSize) != entry.checksum) {
            return {false, entry.sequenceNumber, entry.timestampNs, {}, entry.flags, QStringLiteral("Checksum mismatch")};
        }
        impl_->totalReads.fetch_add(1, std::memory_order_relaxed);
        impl_->totalReadBytes.fetch_add(payloadSize, std::memory_order_relaxed);
        impl_->lastReadTimestampNs.store(monotonicNs(), std::memory_order_relaxed);
        if (impl_->readComplete) impl_->readComplete->release();
        return {true, entry.sequenceNumber, entry.timestampNs, std::move(payload), entry.flags, {}};
    }
}

SharedMemoryRingBuffer::ReadResult SharedMemoryRingBuffer::readBlocking(int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        auto result = read();
        if (result.success || timeoutMs <= 0 || timer.elapsed() >= timeoutMs) return result;
        QThread::msleep(1);
    }
}

SharedMemoryRingBuffer::ReadResult SharedMemoryRingBuffer::readSequence(std::uint64_t sequenceNumber) {
    for (;;) {
        auto result = read();
        if (!result.success || result.sequenceNumber >= sequenceNumber) return result;
    }
}

std::uint64_t SharedMemoryRingBuffer::availableForWrite() const {
    if (!impl_->attached) return 0;
    const auto write = impl_->header->writeIndex.load(std::memory_order_acquire);
    const auto read = impl_->header->readIndex.load(std::memory_order_acquire);
    return impl_->header->totalCapacity - std::min(write - read, impl_->header->totalCapacity);
}

std::uint64_t SharedMemoryRingBuffer::availableForRead() const {
    if (!impl_->attached) return 0;
    const auto write = impl_->header->writeIndex.load(std::memory_order_acquire);
    const auto read = impl_->header->readIndex.load(std::memory_order_acquire);
    return std::min(write - read, impl_->header->totalCapacity);
}

bool SharedMemoryRingBuffer::isEmpty() const { return availableForRead() == 0; }
bool SharedMemoryRingBuffer::isFull() const { return availableForWrite() == 0; }

void SharedMemoryRingBuffer::reset() {
    if (!impl_->attached) return;
    const auto write = impl_->header->writeIndex.load(std::memory_order_acquire);
    impl_->header->readIndex.store(write, std::memory_order_release);
}

void SharedMemoryRingBuffer::close() { if (impl_) impl_->detach(); }
QSystemSemaphore* SharedMemoryRingBuffer::writeSemaphore() { return impl_->writeReady.get(); }
QSystemSemaphore* SharedMemoryRingBuffer::readSemaphore() { return impl_->readComplete.get(); }

SharedMemoryRingBuffer::Stats SharedMemoryRingBuffer::stats() const {
    return {impl_->totalWrites.load(), impl_->totalReads.load(), impl_->totalWriteBytes.load(),
            impl_->totalReadBytes.load(), impl_->droppedWrites.load(),
            impl_->lastWriteTimestampNs.load(), impl_->lastReadTimestampNs.load()};
}

QString SharedMemoryRingBuffer::name() const { return impl_->config.name; }
std::size_t SharedMemoryRingBuffer::capacity() const { return impl_->attached ? impl_->header->totalCapacity : 0; }
bool SharedMemoryRingBuffer::isOpen() const { return impl_->attached; }

}
