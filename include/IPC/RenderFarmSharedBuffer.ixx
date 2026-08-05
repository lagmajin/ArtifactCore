module;
#include <QString>
#include <cstdint>
#include <memory>

export module IPC.RenderFarmSharedBuffer;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore::IPC {

class RenderFarmSharedBuffer {
public:
    struct Config {
        QString bufferName;
        std::size_t totalSizeMB = 1024;
        std::uint32_t maxFrameWidth = 4096;
        std::uint32_t maxFrameHeight = 4096;
    };

    struct FrameWriteResult {
        bool success = false;
        std::uint64_t frameSequence = 0;
        QString error;
    };

    struct FrameReadResult {
        bool success = false;
        std::int64_t frameNumber = 0;
        std::unique_ptr<ImageF32x4_RGBA> frame;
        std::uint64_t timestampNs = 0;
        QString error;
    };

    static std::unique_ptr<RenderFarmSharedBuffer> createConsumer(const Config& config);
    static std::unique_ptr<RenderFarmSharedBuffer> openProducer(const QString& bufferName);

    ~RenderFarmSharedBuffer();
    FrameWriteResult writeFrame(std::int64_t frameNumber, const ImageF32x4_RGBA& frame,
                                std::uint32_t compressionFlags = 0);
    FrameReadResult readNextFrame();
    FrameReadResult readFrame(std::int64_t frameNumber);

    int pendingFrameCount() const;
    std::size_t usedMemoryMB() const;
    std::size_t totalMemoryMB() const;
    float memoryPressure() const;
    std::uint64_t consumerFrameRate() const;
    std::uint64_t producerFrameRate() const;
    void close();

private:
    class Impl;
    explicit RenderFarmSharedBuffer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}
