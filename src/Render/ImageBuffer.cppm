module;
#include <utility>
#include <algorithm>
#include <cmath>

module Render.ImageBuffer:Impl;

import Render.ImageBuffer;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ArtifactCore::RayTrace
{

void ImageBuffer::setPixel(int x, int y, const Color& color, int samplesPerPixel)
{
    float scale = 1.0f / samplesPerPixel;
    int idx = (y * width + x) * 3;
    pixels[idx + 0] = static_cast<std::uint8_t>(std::clamp(std::sqrt(color.x) * scale, 0.0f, 0.999f) * 256);
    pixels[idx + 1] = static_cast<std::uint8_t>(std::clamp(std::sqrt(color.y) * scale, 0.0f, 0.999f) * 256);
    pixels[idx + 2] = static_cast<std::uint8_t>(std::clamp(std::sqrt(color.z) * scale, 0.0f, 0.999f) * 256);
}

bool ImageBuffer::savePNG(const char* filename) const
{
    if (pixels.empty() || width <= 0 || height <= 0)
        return false;

    return stbi_write_png(filename, width, height, 3, pixels.data(), width * 3) != 0;
}

} // namespace ArtifactCore::RayTrace
