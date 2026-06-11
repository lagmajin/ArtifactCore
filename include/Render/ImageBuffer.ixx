module;
#include <cstdint>
#include <string>
#include <vector>

export module Render.ImageBuffer;

import Render.Vector3D;

export namespace ArtifactCore::RayTrace
{

class ImageBuffer
{
public:
    int width = 800;
    int height = 600;
    std::vector<std::uint8_t> pixels;

    ImageBuffer() = default;
    ImageBuffer(int w, int h) : width(w), height(h), pixels(w * h * 3) {}

    void setPixel(int x, int y, const Color& color, int samplesPerPixel = 1);
    bool savePNG(const char* filename) const;
    unsigned char* data() { return pixels.data(); }
};

} // namespace ArtifactCore::RayTrace
