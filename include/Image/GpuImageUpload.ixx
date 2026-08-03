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

struct GpuImageUploadBuffer {
    std::vector<std::uint8_t> bytes;
    int width = 0;
    int height = 0;
    std::size_t rowStride = 0;
    GpuImageFormat format = GpuImageFormat::Rgba32Float;
    SurfaceColorDescriptor descriptor = SurfaceColorDescriptor::unknown();

    bool isValid() const noexcept {
        return !bytes.empty() && width > 0 && height > 0 && rowStride != 0;
    }
};

GpuImageUploadBuffer makeGpuImageUploadBuffer(const ImageSurfaceView& view);

}
