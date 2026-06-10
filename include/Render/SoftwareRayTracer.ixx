module;
#include <utility>
#include <cstdint>
#include <random>
#include <algorithm>
#include <vector>
#include <cmath>

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

export module Render.SoftwareRayTracer;

import Render.Vector3D;
import Render.Ray;
import Render.Camera;
import Render.Sphere;
import Render.Hittable;
import Render.Material;

export namespace ArtifactCore::RayTrace
{

inline float randomFloat()
{
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static thread_local std::mt19937 generator(std::random_device{}());
    return distribution(generator);
}

inline float randomFloat(float min, float max)
{
    return min + (max - min) * randomFloat();
}

inline Vec3 randomVec3() { return { randomFloat(), randomFloat(), randomFloat() }; }
inline Vec3 randomVec3(float min, float max) { return { randomFloat(min, max), randomFloat(min, max), randomFloat(min, max) }; }

inline Vec3 randomInUnitSphere()
{
    while (true)
    {
        Vec3 p = randomVec3(-1.0f, 1.0f);
        if (Vec3::dot(p, p) < 1.0f)
            return p;
    }
}

inline Vec3 randomInHemisphere(const Vec3& normal)
{
    return randomInUnitSphere() + normal;
}

inline Vec3 reflect(const Vec3& v, const Vec3& n)
{
    return v - 2.0f * Vec3::dot(v, n) * n;
}

inline Vec3 refract(const Vec3& uv, const Vec3& n, float etaOverEtaPrime)
{
    float cosTheta = std::clamp(Vec3::dot(-uv, n), -1.0f, 1.0f);
    Vec3 rOutPerp = etaOverEtaPrime * (uv + cosTheta * n);
    Vec3 rOutParallel = -std::sqrt(std::max(0.0f, 1.0f - rOutPerp.lengthSq())) * n;
    return rOutPerp + rOutParallel;
}

inline float schlick(float cosine, float refIdx)
{
    float r0 = (1 - refIdx) / (1 + refIdx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * std::pow(1 - cosine, 5);
}

inline Color rayColor(const Ray& r, const Hittable& world, int depth)
{
    HitRecord rec;
    const Material* mat = nullptr;

    if (depth <= 0)
        return Color(0.0f, 0.0f, 0.0f);

    if (world.hit(r, 0.001f, 1000.0f, rec, mat))
    {
        if (!mat)
            return Color(1.0f, 0.0f, 1.0f); // error color

        switch (mat->type)
        {
        case MaterialType::Lambertian:
        {
            Vec3 target = rec.p + rec.normal + randomInHemisphere(rec.normal);
            Ray scattered(rec.p, target - rec.p);
            return mat->albedo * rayColor(scattered, world, depth - 1);
        }
        case MaterialType::Metal:
        {
            Vec3 reflected = reflect(r.dir.normalized(), rec.normal);
            Ray scattered(rec.p, reflected + mat->fuzz * randomInUnitSphere());
            if (Vec3::dot(scattered.dir, rec.normal) > 0)
                return mat->albedo * rayColor(scattered, world, depth - 1);
            return Color(0.0f, 0.0f, 0.0f);
        }
        case MaterialType::Dielectric:
        {
            float refractionRatio = rec.frontFace ? (1.0f / mat->refIdx) : mat->refIdx;
            Vec3 unitDir = r.dir.normalized();
            float cosTheta = std::clamp(Vec3::dot(-unitDir, rec.normal), -1.0f, 1.0f);
            float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

            bool cannotRefract = refractionRatio * sinTheta > 1.0f;
            Vec3 direction;

            if (cannotRefract || randomFloat() < schlick(cosTheta, refractionRatio))
                direction = reflect(unitDir, rec.normal);
            else
                direction = refract(unitDir, rec.normal, refractionRatio);

            Ray scattered(rec.p, direction);
            return rayColor(scattered, world, depth - 1);
        }
        case MaterialType::DiffuseLight:
            return mat->albedo;
        case MaterialType::Isotropic:
            [[fallthrough]];
        }
    }

    // Background gradient
    Vec3 unitDir = r.dir.normalized();
    float t = 0.5f * (unitDir.y + 1.0f);
    return (1.0f - t) * Color(1.0f, 1.0f, 1.0f) + t * Color(0.5f, 0.7f, 1.0f);
}

class ImageBuffer
{
public:
    int width = 800;
    int height = 600;
    std::vector<uint8_t> pixels;

    ImageBuffer() = default;
    ImageBuffer(int w, int h) : width(w), height(h), pixels(w * h * 3) {}

    void setPixel(int x, int y, const Color& color, int samplesPerPixel = 1)
    {
        float scale = 1.0f / samplesPerPixel;
        int idx = (y * width + x) * 3;
        pixels[idx + 0] = static_cast<uint8_t>(std::clamp(std::sqrt(color.x) * scale, 0.0f, 0.999f) * 256);
        pixels[idx + 1] = static_cast<uint8_t>(std::clamp(std::sqrt(color.y) * scale, 0.0f, 0.999f) * 256);
        pixels[idx + 2] = static_cast<uint8_t>(std::clamp(std::sqrt(color.z) * scale, 0.0f, 0.999f) * 256);
    }

    bool savePNG(const char* filename) const;

    unsigned char* data() { return pixels.data(); }
};

class SoftwareRayTracer
{
public:
    int imageWidth = 800;
    int imageHeight = 600;
    int samplesPerPixel = 100;
    int maxDepth = 50;

    Camera camera;
    HittableList world;

    SoftwareRayTracer()
    {
        camera = Camera(16.0f / 9.0f, 2.0f, 1.0f, 0.0f,
            Point3(0, 0, 0), Point3(0, 0, -1), Vec3(0, 1, 0));
    }

    void setupCornellBox()
    {
        world.clear();

        auto ground = std::make_shared<SphereObject>(
            Sphere{Vec3(0, -100.5f, -1), 100.0f},
            Material::lambertian(Vec3(0.8f, 0.8f, 0.0f))
        );
        world.add(ground);

        auto center = std::make_shared<SphereObject>(
            Sphere{Vec3(0, 0, -1.2f), 0.5f},
            Material::dielectric(1.5f)
        );
        world.add(center);

        auto left = std::make_shared<SphereObject>(
            Sphere{Vec3(-0.9f, 0, -1.2f), 0.5f},
            Material::lambertian(Vec3(0.1f, 0.2f, 0.5f))
        );
        world.add(left);

        auto right = std::make_shared<SphereObject>(
            Sphere{Vec3(0.9f, 0, -1.2f), 0.5f},
            Material::metal(Vec3(0.8f, 0.8f, 0.8f), 0.3f)
        );
        world.add(right);
    }

    void setupRandomSpheres(int count = 500)
    {
        world.clear();

        auto ground = std::make_shared<SphereObject>(
            Sphere{Vec3(0, -1000, 0), 1000},
            Material::lambertian(Vec3(0.5f, 0.5f, 0.5f))
        );
        world.add(ground);

        for (int i = 0; i < count; ++i)
        {
            float a = randomFloat(0.0f, 2.0f * 3.14159265f);
            float radius = randomFloat(0.1f, 0.3f);
            float x = std::cos(a) * randomFloat(0.5f, 5.0f);
            float z = std::sin(a) * randomFloat(0.5f, 5.0f);
            float y = radius - 1.0f + randomFloat(0.1f, 0.5f);

            Vec3 center{x, y, z};
            Sphere s{ center, radius };
            s.id = static_cast<uint32_t>(i) + 1;

            MaterialType types[] = { MaterialType::Lambertian, MaterialType::Metal, MaterialType::Dielectric };
            int typeIdx = static_cast<int>(randomFloat() * 3) % 3;
            Material mat;

            switch (types[typeIdx])
            {
            case MaterialType::Lambertian:
                mat = Material::lambertian(randomVec3(0.3f, 1.0f));
                break;
            case MaterialType::Metal:
                mat = Material::metal(randomVec3(0.3f, 1.0f), randomFloat(0, 0.5f));
                break;
            case MaterialType::Dielectric:
                mat = Material::dielectric(1.5f);
                break;
            default:
                mat = Material::lambertian(Vec3(0.5f, 0.5f, 0.5f));
                break;
            }

            world.add(std::make_shared<SphereObject>(s, mat));
        }
    }

    ImageBuffer render()
    {
        ImageBuffer img(imageWidth, imageHeight);

        camera.setViewport(static_cast<float>(imageWidth), static_cast<float>(imageHeight));

        for (int j = imageHeight - 1; j >= 0; --j)
        {
            for (int i = 0; i < imageWidth; ++i)
            {
                Color pixelColor(0, 0, 0);
                for (int s = 0; s < samplesPerPixel; ++s)
                {
                    float u = (i + randomFloat()) / (imageWidth - 1);
                    float v = (j + randomFloat()) / (imageHeight - 1);

                    Ray r = camera.getRay(u, v);
                    pixelColor = pixelColor + rayColor(r, world, maxDepth);
                }
                img.setPixel(i, j, pixelColor, samplesPerPixel);
            }
        }

        return img;
    }

    void renderParallel(ImageBuffer& img)
    {
        struct RowRange
        {
            int y;
            int width;
            int height;
            int samplesPerPixel;
            int maxDepth;
            const Camera& camera;
            const Hittable& world;
            ImageBuffer& buffer;

            void operator()() const
            {
                for (int i = 0; i < width; ++i)
                {
                    Color pixelColor(0, 0, 0);
                    for (int s = 0; s < samplesPerPixel; ++s)
                    {
                        float u = (i + randomFloat()) / (width - 1);
                        float v = (y + randomFloat()) / (height - 1);

                        Ray r = camera.getRay(u, v);
                        pixelColor = pixelColor + rayColor(r, world, maxDepth);
                    }
                    buffer.setPixel(i, y, pixelColor, samplesPerPixel);
                }
            }
        };

        tbb::parallel_for(tbb::blocked_range<int>(0, imageHeight),
            [&](const tbb::blocked_range<int>& range)
            {
                for (int j = range.begin(); j != range.end(); ++j)
                {
                    RowRange row{ j, imageWidth, imageHeight, samplesPerPixel, maxDepth, camera, world, img };
                    row();
                }
            });
    }
};

} // namespace ArtifactCore::RayTrace