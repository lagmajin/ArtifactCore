module;
#include <cstddef>
#include <cstdint>

export module Image.ImageSurfaceView;

import Graphics.SurfaceColorContract;

export namespace ArtifactCore {

enum class SurfacePrecision : std::uint8_t {
    Float16,
    Float32,
};

struct ImageSurfaceView {
    const void* data = nullptr;
    int width = 0;
    int height = 0;
    std::size_t rowStride = 0;
    SurfacePrecision precision = SurfacePrecision::Float32;
    SurfaceColorDescriptor descriptor = SurfaceColorDescriptor::unknown();

    bool isValid() const noexcept {
        return data != nullptr && width > 0 && height > 0 && rowStride != 0;
    }
};

}
