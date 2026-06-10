module;
#include <utility>
#include <algorithm>

export module Render.Material;

import Render.Vector3D;

export namespace ArtifactCore::RayTrace
{

struct ScatterRecord
{
    Vec3 attenuation;
    Vec3 scatteredDir;
    bool isSpecular = false;
};

enum class MaterialType
{
    Lambertian,
    Metal,
    Dielectric,
    DiffuseLight,
    Isotropic
};

struct Material
{
    MaterialType type = MaterialType::Lambertian;
    Vec3 albedo = {0.5f, 0.5f, 0.5f};
    float fuzz = 0.0f;
    float refIdx = 1.5f;

    Material() = default;
    Material(const Vec3& a) : albedo(a) {}

    static Material lambertian(const Vec3& albedo_)
    {
        Material m;
        m.type = MaterialType::Lambertian;
        m.albedo = albedo_;
        return m;
    }

    static Material metal(const Vec3& albedo_, float fuzz_ = 0.0f)
    {
        Material m;
        m.type = MaterialType::Metal;
        m.albedo = albedo_;
        m.fuzz = std::clamp(fuzz_, 0.0f, 1.0f);
        return m;
    }

    static Material dielectric(float refIdx_)
    {
        Material m;
        m.type = MaterialType::Dielectric;
        m.refIdx = refIdx_;
        return m;
    }

    static Material diffuseLight(const Vec3& emit_)
    {
        Material m;
        m.type = MaterialType::DiffuseLight;
        m.albedo = emit_;
        return m;
    }

    static Material isotropic(const Vec3& albedo_)
    {
        Material m;
        m.type = MaterialType::Isotropic;
        m.albedo = albedo_;
        return m;
    }
};

} // namespace ArtifactCore::RayTrace