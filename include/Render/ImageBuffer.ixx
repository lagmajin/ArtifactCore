module;
#include <cstdint>
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

export module Render.ImageBuffer;

import Render.Vector3D;

export namespace ArtifactCore::RayTrace
{

inline std::size_t imagePixelBytes(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return 0;
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (h > std::numeric_limits<std::size_t>::max() / w) return 0;
    const auto pixels = w * h;
    return pixels > std::numeric_limits<std::size_t>::max() / 3u
        ? 0 : pixels * 3u;
}

class ImageBuffer
{
public:
    int width = 800;
    int height = 600;
    std::vector<std::uint8_t> pixels;

    ImageBuffer() = default;
    ImageBuffer(int w, int h)
        : width(std::max(0, w)), height(std::max(0, h)),
          pixels(imagePixelBytes(std::max(0, w), std::max(0, h))) {}

    void setPixel(int x, int y, const Color& color, int samplesPerPixel = 1);
    bool savePNG(const char* filename) const;
    unsigned char* data() { return pixels.data(); }
};

} // namespace ArtifactCore::RayTrace
