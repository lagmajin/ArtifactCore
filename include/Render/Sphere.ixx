module;
#include <utility>
#include <memory>
#include <cmath>

export module Render.Sphere;

import Render.Vector3D;
import Render.Ray;

export namespace ArtifactCore::RayTrace
{

struct Sphere
{
    Vec3 center;
    float radius = 1.0f;
    uint32_t id = 0;

    Sphere() = default;
    Sphere(const Vec3& center_, float radius_) : center(center_), radius(radius_) {}

    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec) const
    {
        Vec3 oc = center - r.origin;
        float a = Vec3::dot(r.dir, r.dir);
        float b = -2.0f * Vec3::dot(oc, r.dir);
        float c = Vec3::dot(oc, oc) - radius * radius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) return false;

        float sqrtd = std::sqrt(discriminant);

        float root = (-b - sqrtd) / (2.0f * a);
        if (root <= ray_tmin || root >= ray_tmax)
        {
            root = (-b + sqrtd) / (2.0f * a);
            if (root <= ray_tmin || root >= ray_tmax)
                return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outwardNormal = (rec.p - center) / radius;
        rec.setFaceNormal(r, outwardNormal);
        rec.objectId = id;

        return true;
    }
};

using SpherePtr = std::shared_ptr<Sphere>;

} // namespace ArtifactCore::RayTrace