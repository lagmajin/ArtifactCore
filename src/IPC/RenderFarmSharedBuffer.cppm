module;
#include <cstring>
#include <QByteArray>
#include <QString>

module IPC.RenderFarmSharedBuffer;

import IPC.SharedMemoryRingBuffer;

namespace ArtifactCore::IPC {

namespace {
constexpr std::uint32_t kFramePacketMagic = 0x41524650; // ARFP
struct FramePacketHeader {
    std::uint32_t magic = kFramePacketMagic;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t flags = 0;
    std::int64_t frameNumber = 0;
};
}

class RenderFarmSharedBuffer::Impl {
public:
    Config config;
    std::unique_ptr<SharedMemoryRingBuffer> ring;
    bool consumer = false;

    explicit Impl(const Config& value, bool isConsumer) : config(value), consumer(isConsumer) {}
};

RenderFarmSharedBuffer::RenderFarmSharedBuffer(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
RenderFarmSharedBuffer::~RenderFarmSharedBuffer() { close(); }

std::unique_ptr<RenderFarmSharedBuffer> RenderFarmSharedBuffer::createConsumer(const Config& config) {
    if (config.bufferName.trimmed().isEmpty() || config.totalSizeMB == 0) return {};
    SharedMemoryRingBuffer::Config ringConfig;
    ringConfig.name = config.bufferName;
    ringConfig.totalSize = config.totalSizeMB * 1024ull * 1024ull;
    ringConfig.maxEntrySize = sizeof(FramePacketHeader) +
        static_cast<std::size_t>(config.maxFrameWidth) * config.maxFrameHeight * 4u * sizeof(float);
    ringConfig.create = true;
    auto ring = SharedMemoryRingBuffer::create(ringConfig);
    if (!ring) return {};
    auto impl = std::make_unique<Impl>(config, true);
    impl->ring = std::move(ring);
    return std::unique_ptr<RenderFarmSharedBuffer>(new RenderFarmSharedBuffer(std::move(impl)));
}

std::unique_ptr<RenderFarmSharedBuffer> RenderFarmSharedBuffer::openProducer(const QString& bufferName) {
    auto ring = SharedMemoryRingBuffer::open(bufferName);
    if (!ring) return {};
    Config config;
    config.bufferName = bufferName;
    config.totalSizeMB = ring->capacity() / (1024ull * 1024ull);
    auto impl = std::make_unique<Impl>(config, false);
    impl->ring = std::move(ring);
    return std::unique_ptr<RenderFarmSharedBuffer>(new RenderFarmSharedBuffer(std::move(impl)));
}

RenderFarmSharedBuffer::FrameWriteResult RenderFarmSharedBuffer::writeFrame(
    std::int64_t frameNumber, const ImageF32x4_RGBA& frame, std::uint32_t compressionFlags) {
    if (!impl_ || !impl_->ring || impl_->consumer || frame.isEmpty()) return {false, 0, QStringLiteral("Invalid producer or frame")};
    const std::size_t pixelBytes = static_cast<std::size_t>(frame.width()) * frame.height() * 4u * sizeof(float);
    if (pixelBytes > impl_->ring->capacity()) return {false, 0, QStringLiteral("Frame exceeds shared buffer capacity")};
    FramePacketHeader header;
    header.width = static_cast<std::uint32_t>(frame.width());
    header.height = static_cast<std::uint32_t>(frame.height());
    header.flags = compressionFlags;
    header.frameNumber = frameNumber;
    QByteArray packet(static_cast<qsizetype>(sizeof(header) + pixelBytes), Qt::Uninitialized);
    std::memcpy(packet.data(), &header, sizeof(header));
    std::memcpy(packet.data() + sizeof(header), frame.rgba32fData(), pixelBytes);
    const auto result = impl_->ring->write(packet);
    return {result.success, result.sequenceNumber, result.error};
}

RenderFarmSharedBuffer::FrameReadResult RenderFarmSharedBuffer::readNextFrame() {
    if (!impl_ || !impl_->ring || !impl_->consumer) return {false, 0, {}, 0, QStringLiteral("Invalid consumer")};
    const auto result = impl_->ring->read();
    if (!result.success) return {false, 0, {}, 0, result.error};
    if (result.data.size() < static_cast<qsizetype>(sizeof(FramePacketHeader))) return {false, 0, {}, result.timestampNs, QStringLiteral("Invalid frame packet")};
    FramePacketHeader header{};
    std::memcpy(&header, result.data.constData(), sizeof(header));
    const std::size_t expected = sizeof(header) + static_cast<std::size_t>(header.width) * header.height * 4u * sizeof(float);
    if (header.magic != kFramePacketMagic || expected != static_cast<std::size_t>(result.data.size())) return {false, 0, {}, result.timestampNs, QStringLiteral("Invalid frame dimensions")};
    auto frame = std::make_unique<ImageF32x4_RGBA>();
    frame->setFromRGBA32F(reinterpret_cast<const float*>(result.data.constData() + sizeof(header)),
                          static_cast<int>(header.width), static_cast<int>(header.height));
    return {true, header.frameNumber, std::move(frame), result.timestampNs, {}};
}

RenderFarmSharedBuffer::FrameReadResult RenderFarmSharedBuffer::readFrame(std::int64_t frameNumber) {
    for (;;) {
        auto result = readNextFrame();
        if (!result.success || result.frameNumber >= frameNumber) return result;
    }
}

int RenderFarmSharedBuffer::pendingFrameCount() const {
    return impl_ && impl_->ring ? static_cast<int>(impl_->ring->availableForRead() > 0 ? 1 : 0) : 0;
}
std::size_t RenderFarmSharedBuffer::usedMemoryMB() const {
    if (!impl_ || !impl_->ring) return 0;
    return static_cast<std::size_t>((impl_->ring->capacity() - impl_->ring->availableForWrite()) / (1024ull * 1024ull));
}
std::size_t RenderFarmSharedBuffer::totalMemoryMB() const {
    return impl_ && impl_->ring ? impl_->ring->capacity() / (1024ull * 1024ull) : 0;
}
float RenderFarmSharedBuffer::memoryPressure() const {
    const auto total = totalMemoryMB();
    return total == 0 ? 0.0f : static_cast<float>(usedMemoryMB()) / static_cast<float>(total);
}
std::uint64_t RenderFarmSharedBuffer::consumerFrameRate() const { return 0; }
std::uint64_t RenderFarmSharedBuffer::producerFrameRate() const { return 0; }
void RenderFarmSharedBuffer::close() { if (impl_ && impl_->ring) impl_->ring->close(); }

}
