module;
#include <utility>
#include <cmath>

export module Render.Camera;

import Render.Vector3D;
import Render.Ray;

export namespace ArtifactCore::RayTrace
{

class Camera
{
public:
    float aspectRatio = 1.0f;
    float viewportHeight = 2.0f;
    float viewportWidth = 0.0f;
    float focalLength = 1.0f;

    Point3 origin;
    Point3 lowerLeftCorner;
    Vec3 horizontal;
    Vec3 vertical;

    Camera() : origin(Point3(0, 0, 0)), lowerLeftCorner(Vec3(-2, -1, -1)),
               horizontal(Vec3(4, 0, 0)), vertical(Vec3(0, 2, 0)) {}

    Camera(float aspect, float vfov, float focusDist, float aperture,
           const Point3& lookFrom, const Point3& lookAt, const Vec3& vup)
    {
        float theta = vfov * 3.14159265f / 180.0f;
        float h = std::tan(theta / 2.0f);
        viewportHeight = 2.0f * h;
        viewportWidth = aspect * viewportHeight;

        origin = lookFrom;
        Vec3 w = (lookFrom - lookAt).normalized();
        Vec3 u = Vec3::cross(vup, w).normalized();
        Vec3 v = Vec3::cross(w, u);

        lowerLeftCorner = origin - focusDist * (u * viewportWidth / 2.0f + v * viewportHeight / 2.0f + w * focusDist);
        horizontal = u * viewportWidth * focusDist;
        vertical = v * viewportHeight * focusDist;
    }

    Ray getRay(float u, float v) const
    {
        return Ray(origin, lowerLeftCorner + u * horizontal + v * vertical - origin);
    }

    void setViewport(float width, float height)
    {
        aspectRatio = width / height;
        viewportWidth = aspectRatio * viewportHeight;
    }
};

} // namespace ArtifactCore::RayTrace