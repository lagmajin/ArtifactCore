module;
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "../Define/DllExportMacro.hpp"

export module Video.VideoFrame;

import Memory.SharedPtr;

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
        if (meta.width <= 0 || meta.height <= 0 || bytes.empty() || strideBytes <= 0) {
            return false;
        }

        std::size_t bytesPerPixel = 0;
        switch (meta.pixelFormat) {
        case VideoFramePixelFormat::RGB24:
            bytesPerPixel = 3;
            break;
        case VideoFramePixelFormat::RGBA8:
        case VideoFramePixelFormat::BGRA8:
            bytesPerPixel = 4;
            break;
        case VideoFramePixelFormat::RGBA32F:
            bytesPerPixel = 4 * sizeof(float);
            break;
        case VideoFramePixelFormat::YUV420P:
        case VideoFramePixelFormat::NV12:
            bytesPerPixel = 1;
            break;
        case VideoFramePixelFormat::Unknown:
        case VideoFramePixelFormat::VulkanImage:
            return false;
        }

        const auto maxSize = std::numeric_limits<std::size_t>::max();
        const auto width = static_cast<std::size_t>(meta.width);
        const auto height = static_cast<std::size_t>(meta.height);
        const auto stride = static_cast<std::size_t>(strideBytes);
        if (width > maxSize / bytesPerPixel || stride < width * bytesPerPixel ||
            height > maxSize / stride) {
            return false;
        }
        return bytes.size() >= stride * height;
    }
};

struct VulkanVideoFrameHandle {
    void* image = nullptr;
    void* memory = nullptr;
    void* semaphore = nullptr;
    std::uint64_t semaphoreValue = 0;
    std::uint32_t nativeFormat = 0;
    std::uint32_t imageLayout = 0;
    std::uint32_t planeCount = 1;
    std::uint32_t planeIndex = 0;
    std::uint32_t arrayLayer = 0;
};

// Native D3D12 decoder output. The owning AVFrame is retained by GpuVideoFrame::lifetime;
// these pointers are borrowed from that frame and must not outlive it.
struct D3D12VideoFrameHandle {
    void* texture = nullptr;
    void* fence = nullptr;
    std::uint64_t fenceValue = 0;
    std::uint32_t subresourceIndex = 0;
};

struct GpuVideoFrame {
    VideoFrameMetadata meta;
    VideoFrameStorageKind storage = VideoFrameStorageKind::Unknown;
    std::variant<std::monostate, VulkanVideoFrameHandle, D3D12VideoFrameHandle> handle;
    SharedPtr<void> lifetime;

    bool isValid() const
    {
        if (meta.width <= 0 || meta.height <= 0) {
            return false;
        }
        if (storage == VideoFrameStorageKind::D3D12Texture) {
            const auto* d3d12 = std::get_if<D3D12VideoFrameHandle>(&handle);
            return d3d12 && d3d12->texture && d3d12->fence && lifetime;
        }
        if (storage == VideoFrameStorageKind::VulkanImage) {
            const auto* vulkan = std::get_if<VulkanVideoFrameHandle>(&handle);
            return vulkan && vulkan->image && lifetime;
        }
        return false;
    }
};

using DecodedVideoFrame = std::variant<std::monostate, CpuVideoFrame, GpuVideoFrame>;

} // namespace ArtifactCore
