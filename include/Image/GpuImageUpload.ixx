module;
#include <cstddef>
#include <cstdint>
#include <vector>

export module Image.GpuImageUpload;

import Image.ImageSurfaceView;
import Graphics.SurfaceColorContract;

export namespace ArtifactCore {

enum class GpuImageFormat : std::uint8_t {
    Rgba16Float,
    Rgba32Float,
};

enum class GpuImageChannelOrder : std::uint8_t {
    Rgba,
    Bgra,
};

struct GpuImageUploadBuffer {
    std::vector<std::uint8_t> bytes;
    const void* sourceData = nullptr;
    int width = 0;
    int height = 0;
    std::size_t rowStride = 0;
    GpuImageFormat format = GpuImageFormat::Rgba32Float;
    // GPU image uploads require the canonical BGRA value order and preserve it.
    SurfaceColorDescriptor descriptor = SurfaceColorDescriptor::unknown();

    const void* data() const noexcept {
        return bytes.empty() ? sourceData : bytes.data();
    }

    bool isValid() const noexcept {
        return data() != nullptr && width > 0 && height > 0 && rowStride != 0;
    }
};

GpuImageUploadBuffer makeGpuImageUploadBuffer(const ImageSurfaceView& view);
GpuImageUploadBuffer makeGpuImageUploadBuffer(
    const ImageSurfaceView& view, GpuImageChannelOrder targetOrder);
bool makeGpuImageUploadBuffer(const ImageSurfaceView& view,
                              GpuImageUploadBuffer& reusableBuffer);
bool makeGpuImageUploadBuffer(const ImageSurfaceView& view,
                              GpuImageChannelOrder targetOrder,
                              GpuImageUploadBuffer& reusableBuffer);

}
