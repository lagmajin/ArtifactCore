module;
#include <memory>

export module Render.GPURayTracer;

import Render.IRayTracer;
import Render.Camera;
import Render.Hittable;
import Render.SoftwareRayTracer; // For ImageBuffer

export namespace ArtifactCore::RayTrace
{

class GPURayTracer : public IRayTracer
{
public:
    int imageWidth = 800;
    int imageHeight = 600;
    int samplesPerPixel = 100;
    int maxDepth = 50;

    Camera camera;
    HittableList world;

    GPURayTracer();
    ~GPURayTracer() override;

    void setImageSize(int width, int height) override;
    void setSamplesPerPixel(int samples) override;
    void setMaxDepth(int depth) override;

    void setupCornellBox() override;
    void setupRandomSpheres(int count = 500) override;

    ImageBuffer render() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore::RayTrace
