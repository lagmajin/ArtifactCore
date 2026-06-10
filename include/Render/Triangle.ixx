module;
#include <utility>

export module Render.Triangle;

import Render.Vector3D;
import Render.Ray;

export namespace ArtifactCore::RayTrace
{

struct Triangle
{
    Vec3 v0, v1, v2;
    Vec3 normal;
    uint32_t id = 0;

    Triangle() = default;
    Triangle(const Vec3& a, const Vec3& b, const Vec3& c) : v0(a), v1(b), v2(c)
    {
        normal = Vec3::cross(v1 - v0, v2 - v0).normalized();
    }

    bool hit(const Ray& r, float ray_tmin, float ray_tmax, HitRecord& rec) const
    {
        float denom = Vec3::dot(normal, r.dir);
        float invDenom = 1.0f / denom;

        float t = Vec3::dot(v0 - r.origin, normal) * invDenom;
        if (t < ray_tmin || t > ray_tmax)
            return false;

        Vec3 p = r.at(t);
        Vec3 bary = p - v0;

        float d00 = Vec3::dot(v1 - v0, v1 - v0);
        float d01 = Vec3::dot(v1 - v0, v2 - v0);
        float d11 = Vec3::dot(v2 - v0, v2 - v0);
        float d20 = Vec3::dot(bary, v1 - v0);
        float d21 = Vec3::dot(bary, v2 - v0);

        float denomB = d00 * d11 - d01 * d01;
        float v = (d11 * d20 - d01 * d21) / denomB;
        float w = (d00 * d21 - d01 * d20) / denomB;
        float u = 1.0f - v - w;

        if (u < 0.0f || v < 0.0f || w < 0.0f)
            return false;

        rec.t = t;
        rec.p = p;
        rec.normal = normal;
        rec.frontFace = Vec3::dot(r.dir, normal) < 0;
        rec.objectId = id;

        return true;
    }
};

} // namespace ArtifactCore::RayTrace