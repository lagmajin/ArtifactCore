module;
#include <utility>
#include <memory>

export module Render.FrameRenderer;

import Render.Vector3D;
import Render.Ray;
import Render.Camera;
import Render.Hittable;
import Render.IRayTracer;
import Render.SoftwareRayTracer;
import Render.GPURayTracer;
import Render.ImageBuffer;

export namespace ArtifactCore
{

enum class RayTracerType
{
    Software,
    GPU
};

class FrameRenderer
{
public:
    std::unique_ptr<RayTrace::IRayTracer> rayTracer;
    RayTracerType currentType = RayTracerType::Software;

    int width = 800;
    int height = 600;

    FrameRenderer()
    {
        setRayTracerType(RayTracerType::Software);
    }

    FrameRenderer(int w, int h) : width(w), height(h)
    {
        setRayTracerType(RayTracerType::Software);
    }

    void setRayTracerType(RayTracerType type)
    {
        currentType = type;
        if (type == RayTracerType::Software)
        {
            rayTracer = std::make_unique<RayTrace::SoftwareRayTracer>();
        }
        else
        {
            rayTracer = std::make_unique<RayTrace::GPURayTracer>();
        }
        rayTracer->setImageSize(width, height);
    }

    RayTrace::ImageBuffer renderFrame()
    {
        if (!rayTracer)
            return RayTrace::ImageBuffer(width, height);
        return rayTracer->render();
    }

    bool savePNG(const char* filename)
    {
        auto buffer = renderFrame();
        return buffer.savePNG(filename);
    }
};

} // namespace ArtifactCore
