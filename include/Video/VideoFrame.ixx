module;
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "../Define/DllExportMacro.hpp"

export module Video.VideoFrame;

export namespace ArtifactCore {

enum class VideoFramePixelFormat {
    Unknown = 0,
    RGB24,
    RGBA8,
    BGRA8,
    RGBA32F,
    YUV420P,
    NV12,
    VulkanImage
};

enum class VideoFrameStorageKind {
    Unknown = 0,
    CpuMemory,
    VulkanImage,
    D3D12Texture
};

struct VideoFrameColorInfo {
    int colorSpace = 0;
    int colorRange = 0;
    int colorPrimaries = 0;
    int colorTransfer = 0;
};

struct VideoFrameMetadata {
    int width = 0;
    int height = 0;
    int64_t pts = -1;
    int64_t duration = -1;
    double timestampSeconds = 0.0;
    VideoFramePixelFormat pixelFormat = VideoFramePixelFormat::Unknown;
    VideoFrameColorInfo color;
};

struct CpuVideoFrame {
    VideoFrameMetadata meta;
    std::vector<std::uint8_t> bytes;
    int strideBytes = 0;

    bool isValid() const
    {
        return meta.width > 0 && meta.height > 0 && !bytes.empty() && strideBytes > 0;
    }
};

struct VulkanVideoFrameHandle {
    void* image = nullptr;
    void* memory = nullptr;
    void* semaphore = nullptr;
    std::uint64_t semaphoreValue = 0;
};

struct GpuVideoFrame {
    VideoFrameMetadata meta;
    VideoFrameStorageKind storage = VideoFrameStorageKind::Unknown;
    std::variant<std::monostate, VulkanVideoFrameHandle> handle;

    bool isValid() const
    {
        return meta.width > 0 && meta.height > 0 && storage != VideoFrameStorageKind::Unknown &&
               !std::holds_alternative<std::monostate>(handle);
    }
};

using DecodedVideoFrame = std::variant<std::monostate, CpuVideoFrame, GpuVideoFrame>;

} // namespace ArtifactCore
