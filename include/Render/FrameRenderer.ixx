module;
#include <utility>

export module Render.FrameRenderer;

import Render.Vector3D;
import Render.Ray;
import Render.Camera;
import Render.Hittable;
import Render.SoftwareRayTracer;

export namespace ArtifactCore
{

class FrameRenderer
{
public:
    RayTrace::SoftwareRayTracer rayTracer;

    int width = 800;
    int height = 600;

    FrameRenderer() = default;
    FrameRenderer(int w, int h) : width(w), height(h)
    {
        rayTracer.imageWidth = w;
        rayTracer.imageHeight = h;
    }

    RayTrace::ImageBuffer renderFrame()
    {
        return rayTracer.render();
    }

    bool savePNG(const char* filename)
    {
        auto buffer = renderFrame();
        return buffer.savePNG(filename);
    }
};

} // namespace ArtifactCore