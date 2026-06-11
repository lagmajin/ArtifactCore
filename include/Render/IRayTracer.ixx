module;
#include <memory>

export module Render.IRayTracer;

import Render.ImageBuffer;
import Render.Camera;
import Render.Hittable;

export namespace ArtifactCore::RayTrace
{

class IRayTracer
{
public:
    virtual ~IRayTracer() = default;

    virtual void setImageSize(int width, int height) = 0;
    virtual void setSamplesPerPixel(int samples) = 0;
    virtual void setMaxDepth(int depth) = 0;

    virtual void setupCornellBox() = 0;
    virtual void setupRandomSpheres(int count = 500) = 0;

    virtual ImageBuffer render() = 0;
};

} // namespace ArtifactCore::RayTrace
