module;
#include <utility>
#include <vector>
#include <memory>

export module Render.Hittable;

import Render.Vector3D;
import Render.Ray;
import Render.Sphere;
import Render.Material;
import Render.Triangle;

export namespace ArtifactCore::RayTrace
{

class Hittable
{
public:
    virtual ~Hittable() = default;
    virtual bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec, const Material*& mat) const = 0;
};

using HittablePtr = std::shared_ptr<Hittable>;

class SphereObject : public Hittable
{
public:
    Sphere sphere;
    Material material;

    SphereObject(const Sphere& s, const Material& m) : sphere(s), material(m) {}

    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec, const Material*& mat) const override
    {
        if (sphere.hit(r, ray_tmin, ray_tmax, rec))
        {
            mat = &material;
            return true;
        }
        return false;
    }
};

class HittableList : public Hittable
{
public:
    std::vector<HittablePtr> objects;

    HittableList() = default;
    explicit HittableList(const HittablePtr& obj) { add(obj); }

    void add(const HittablePtr& obj) { objects.push_back(obj); }
    void clear() { objects.clear(); }
    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec, const Material*& mat) const override
    {
        HitRecord tmp;
        const Material* tmpMat = nullptr;
        float closestSoFar = ray_tmax;
        bool hitAnything = false;

        for (const auto& obj : objects)
        {
            if (obj->hit(r, ray_tmin, closestSoFar, tmp, tmpMat))
            {
                hitAnything = true;
                closestSoFar = tmp.t;
                rec = tmp;
                mat = tmpMat;
            }
        }

        return hitAnything;
    }
};

class TriangleObject : public Hittable
{
public:
    Triangle triangle;
    Material material;

    TriangleObject(const Triangle& t, const Material& m) : triangle(t), material(m) {}

    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec, const Material*& mat) const override
    {
        if (triangle.hit(r, ray_tmin, ray_tmax, rec))
        {
            mat = &material;
            return true;
        }
        return false;
    }
};

} // namespace ArtifactCore::RayTrace