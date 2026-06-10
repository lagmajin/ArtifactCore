module;
#include <utility>

module Render.SoftwareRayTracer:Impl;

import Render.SoftwareRayTracer;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ArtifactCore::RayTrace
{

bool ImageBuffer::savePNG(const char* filename) const
{
    if (pixels.empty() || width <= 0 || height <= 0)
        return false;

    return stbi_write_png(filename, width, height, 3, pixels.data(), width * 3) != 0;
}

} // namespace ArtifactCore::RayTrace