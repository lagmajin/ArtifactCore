module;
#include <cstdint>
#include <cstddef>
#include <vector>

export module Image.ImageF16x4;

import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Image.ImageSurfaceView;

export namespace ArtifactCore {

class ImageF16x4 {
public:
    ImageF16x4() = default;
    ImageF16x4(int width, int height,
               SurfaceColorDescriptor descriptor =
                   SurfaceColorDescriptor::linearStraightRgba16Float());

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool isEmpty() const noexcept { return pixels_.empty(); }
    const std::uint16_t* data() const noexcept { return pixels_.data(); }
    std::uint16_t* data() noexcept { return pixels_.data(); }
    std::size_t size() const noexcept { return pixels_.size(); }

    SurfaceColorDescriptor colorDescriptor() const noexcept { return descriptor_; }
    void setColorDescriptor(const SurfaceColorDescriptor& descriptor) noexcept {
        descriptor_ = descriptor;
    }

    static ImageF16x4 fromF32(const ImageF32x4_RGBA& source);
    ImageF32x4_RGBA toF32() const;
    ImageF16x4 toCanonicalRGBA16FC4() const;
    ImageF16x4 toCanonicalBGRA16FC4() const;
    ImageSurfaceView surfaceView() const noexcept;

private:
    int width_ = 0;
    int height_ = 0;
    SurfaceColorDescriptor descriptor_ = SurfaceColorDescriptor::unknown();
    std::vector<std::uint16_t> pixels_;
};

}
