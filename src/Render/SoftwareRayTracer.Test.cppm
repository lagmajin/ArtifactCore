module;
#include <utility>
#include <iostream>
#include <fstream>
#include <cmath>

module Render.SoftwareRayTracer.Test;

import Render.Vector3D;
import Render.Ray;
import Render.Camera;
import Render.Hittable;
import Render.Material;
import Render.Sphere;
import Render.SoftwareRayTracer;

namespace ArtifactCore::RayTrace::Test
{

bool raySphereIntersectionTest()
{
    Vec3 origin(0, 0, 0);
    Vec3 dir(0, 0, -1);
    Ray ray(origin, dir);

    Sphere sphere(Vec3(0, 0, -5), 1.0f);
    HitRecord rec;

    bool hit = sphere.hit(ray, 0.001f, 1000.0f, rec);

    if (!hit)
    {
        std::cerr << "FAIL: Ray should hit sphere\n";
        return false;
    }

    if (std::abs(rec.t - 4.0f) > 0.001f)
    {
        std::cerr << "FAIL: Expected t=4, got " << rec.t << "\n";
        return false;
    }

    std::cout << "PASS: raySphereIntersectionTest\n";
    return true;
}

bool lambertianMaterialTest()
{
    HittableList world;
    world.add(std::make_shared<SphereObject>(
        Sphere(Vec3(0, 0, -5), 1.0f),
        Material::lambertian(Vec3(0.5f, 0.5f, 0.5f))
    ));

    Ray ray(Vec3(0, 0, 0), Vec3(0, 0, -1));
    HitRecord rec;
    const Material* mat = nullptr;

    bool hit = world.hit(ray, 0.001f, 1000.0f, rec, mat);

    if (!hit || !mat || mat->type != MaterialType::Lambertian)
    {
        std::cerr << "FAIL: Lambertian material not working\n";
        return false;
    }

    std::cout << "PASS: lambertianMaterialTest\n";
    return true;
}

bool mcRenderTest(int width = 200, int height = 200)
{
    SoftwareRayTracer tracer;
    tracer.setupCornellBox();
    tracer.imageWidth = width;
    tracer.imageHeight = height;
    tracer.samplesPerPixel = 10;

    auto buffer = tracer.render();

    if (buffer.width <= 0 || buffer.height <= 0)
    {
        std::cerr << "FAIL: Render produced empty image\n";
        return false;
    }

    if (buffer.pixels.size() != static_cast<size_t>(width * height * 3))
    {
        std::cerr << "FAIL: Pixel buffer size mismatch\n";
        return false;
    }

    std::cout << "PASS: mcRenderTest (" << width << "x" << height << ")\n";
    return true;
}

bool savePNTest(const std::string& path = "test_output.png")
{
    SoftwareRayTracer tracer;
    tracer.setupCornellBox();
    tracer.imageWidth = 100;
    tracer.imageHeight = 100;
    tracer.samplesPerPixel = 50;

    auto buffer = tracer.render();
    bool saved = buffer.savePNG(path.c_str());

    if (!saved)
    {
        std::cerr << "FAIL: Could not save PNG to " << path << "\n";
        return false;
    }

    std::cout << "PASS: savePNTest (" << path << ")\n";
    return true;
}

int runAllTests()
{
    int passed = 0, failed = 0;

    if (raySphereIntersectionTest()) ++passed; else ++failed;
    if (lambertianMaterialTest()) ++passed; else ++failed;
    if (mcRenderTest()) ++passed; else ++failed;
    if (savePNTest()) ++passed; else ++failed;

    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Passed: " << passed << ", Failed: " << failed << "\n";

    return failed > 0 ? 1 : 0;
}

} // namespace ArtifactCore::RayTrace::Test

int main()
{
    return ArtifactCore::RayTrace::Test::runAllTests();
}