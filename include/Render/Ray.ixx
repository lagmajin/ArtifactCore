module;
#include <utility>
#include <cstdint>
#include <limits>

export module Render.Ray;

import Render.Vector3D;

export namespace ArtifactCore::RayTrace
{

struct Ray
{
    Point3 origin;
    Vec3 dir;

    Ray() = default;
    Ray(const Point3& origin_, const Vec3& dir_) : origin(origin_), dir(dir_) {}

    Point3 at(float t) const { return origin + t * dir; }
};

struct Interval
{
    float min, max;

    Interval() : min(0), max(0) {}
    Interval(float min_, float max_) : min(min_), max(max_) {}

    float clamp(float x) const
    {
        return x < min ? min : (x > max ? max : x);
    }

    static const Interval empty;
    static const Interval universe;
};

inline const Interval Interval::empty = Interval(std::numeric_limits<float>::infinity(),
                                              -std::numeric_limits<float>::infinity());
inline const Interval Interval::universe = Interval(-std::numeric_limits<float>::infinity(),
                                                  std::numeric_limits<float>::infinity());

struct HitRecord
{
    Point3 p;
    Vec3 normal;
    float t = 0.0f;
    bool frontFace = true;
    uint32_t objectId = 0;

    void setFaceNormal(const Ray& r, const Vec3& outwardNormal)
    {
        frontFace = Vec3::dot(r.dir, outwardNormal) < 0;
        normal = frontFace ? outwardNormal : -outwardNormal;
    }
};

} // namespace ArtifactCore::RayTrace