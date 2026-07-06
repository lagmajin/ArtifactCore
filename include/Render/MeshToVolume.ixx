module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

export module Render.MeshToVolume;

import Render.Vector3D;
import Render.Triangle;
import Render.VolumeRenderer;

export namespace ArtifactCore::RayTrace {

struct MeshToVolumeSettings {
    float surfaceThickness = 0.05f;
    float interiorDensity = 1.0f;
    float surfaceFalloff = 0.1f;
    bool fillInterior = true;
    bool smoothNormals = true;
};

struct SimpleTriangle {
    Vec3 v0, v1, v2;
};

struct SimpleMesh {
    std::vector<SimpleTriangle> triangles;
};

class MeshToVolumeConverter {
public:
    MeshToVolumeConverter() = default;

    void setMesh(const std::vector<SimpleTriangle>& triangles);
    void setMesh(const Triangle* triangles, std::size_t count);
    void setSettings(const MeshToVolumeSettings& settings);

    [[nodiscard]] bool convertToScalarField(const VolumeAABB& bounds, VolumeScalarField& field) const noexcept;

private:
    std::vector<SimpleTriangle> triangles_;
    MeshToVolumeSettings settings_;

    [[nodiscard]] float pointToTriangleDistance(const Vec3& p, const SimpleTriangle& tri) const noexcept;
    [[nodiscard]] bool pointInsideMesh(const Vec3& p) const noexcept;
};

[[nodiscard]] float signedDistanceToVolumeDensity(float signedDist, const MeshToVolumeSettings& settings) noexcept;

} // namespace ArtifactCore::RayTrace