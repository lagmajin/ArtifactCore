module;
#include <utility>
#include <cmath>
#include <random>

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
    Vec3 uAxis{1.0f, 0.0f, 0.0f};
    Vec3 vAxis{0.0f, 1.0f, 0.0f};
    Vec3 wAxis{0.0f, 0.0f, 1.0f};
    float focusDistance = 1.0f;
    float aperture = 0.0f;
    float sensorWidth = 0.036f;

    Camera() : origin(Point3(0, 0, 0)), lowerLeftCorner(Vec3(-2, -1, -1)),
               horizontal(Vec3(4, 0, 0)), vertical(Vec3(0, 2, 0)) {}

    Camera(float aspect, float vfov, float focusDist, float aperture_,
           const Point3& lookFrom, const Point3& lookAt, const Vec3& vup)
    {
        float theta = vfov * 3.14159265f / 180.0f;
        float h = std::tan(theta / 2.0f);
        viewportHeight = 2.0f * h;
        viewportWidth = aspect * viewportHeight;

        origin = lookFrom;
        focusDistance = focusDist;
        aperture = aperture_;

        wAxis = (lookFrom - lookAt).normalized();
        uAxis = Vec3::cross(vup, wAxis).normalized();
        vAxis = Vec3::cross(wAxis, uAxis);

        lowerLeftCorner = origin - focusDist * (uAxis * viewportWidth / 2.0f + vAxis * viewportHeight / 2.0f + wAxis * focusDist);
        horizontal = uAxis * viewportWidth * focusDist;
        vertical = vAxis * viewportHeight * focusDist;
    }

    Ray getRay(float u, float v) const
    {
        return Ray(origin, lowerLeftCorner + u * horizontal + v * vertical - origin);
    }

    Ray getRayDOF(float u, float v, float& lensU, float& lensV) const
    {
        const Point3 lensPoint = origin + uAxis * lensU * aperture * 0.5f + vAxis * lensV * aperture * 0.5f;
        const Point3 focusPoint = origin + (lowerLeftCorner + u * horizontal + v * vertical - origin) * focusDistance;
        return Ray(lensPoint, focusPoint - lensPoint);
    }

    void setViewport(float width, float height)
    {
        aspectRatio = width / height;
        viewportWidth = aspectRatio * viewportHeight;
    }

    void setDOF(float aperture_, float focusDist, float sensorWidth_ = 0.036f)
    {
        aperture = aperture_;
        focusDistance = focusDist;
        sensorWidth = sensorWidth_;
    }
};

} // namespace ArtifactCore::RayTrace