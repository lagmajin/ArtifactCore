module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

module Render.MeshToVolume;

namespace ArtifactCore::RayTrace {

namespace {

inline float dot3(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross3(const Vec3& a, const Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vec3 sub3(const Vec3& a, const Vec3& b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 add3(const Vec3& a, const Vec3& b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 scale3(const Vec3& v, float s) noexcept {
    return {v.x * s, v.y * s, v.z * s};
}

inline float length3(const Vec3& v) noexcept {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

}

void MeshToVolumeConverter::setMesh(const std::vector<SimpleTriangle>& triangles) {
    triangles_ = triangles;
}

void MeshToVolumeConverter::setMesh(const Triangle* triangles, std::size_t count) {
    triangles_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        triangles_[i].v0 = triangles[i].v0;
        triangles_[i].v1 = triangles[i].v1;
        triangles_[i].v2 = triangles[i].v2;
    }
}

void MeshToVolumeConverter::setSettings(const MeshToVolumeSettings& settings) {
    settings_ = settings;
}

float MeshToVolumeConverter::pointToTriangleDistance(const Vec3& p, const SimpleTriangle& tri) const noexcept {
    const Vec3 e0 = sub3(tri.v1, tri.v0);
    const Vec3 e1 = sub3(tri.v2, tri.v0);
    const Vec3 d = sub3(tri.v0, p);

    const float a = dot3(e0, e0);
    const float b = dot3(e0, e1);
    const float c = dot3(e1, e1);
    const float de = dot3(d, e0);
    const float df = dot3(d, e1);

    const float det = std::abs(a * c - b * b);
    float s = b * df - c * de;
    float t = b * de - a * df;

    if (s + t <= det) {
        if (s < 0.0f) {
            if (t < 0.0f) {
                const float sqDist = d.x * d.x + d.y * d.y + d.z * d.z;
                return std::sqrt(sqDist);
            }
            s = 0.0f;
            t = std::clamp(-df / std::max(c, 1e-12f), 0.0f, 1.0f);
        } else if (t < 0.0f) {
            t = 0.0f;
            s = std::clamp(-de / std::max(a, 1e-12f), 0.0f, 1.0f);
        } else {
            const float invDet = 1.0f / std::max(det, 1e-12f);
            s *= invDet;
            t *= invDet;
        }
    } else {
        if (s < 0.0f) {
            const float tmp0 = b + de;
            const float tmp1 = c + df;
            if (tmp1 > tmp0) {
                const float numer = tmp1 - tmp0;
                const float denom = a - 2.0f * b + c;
                s = std::clamp(numer / std::max(denom, 1e-12f), 0.0f, 1.0f);
                t = 1.0f - s;
            } else {
                s = 0.0f;
                t = std::clamp(-df / std::max(c, 1e-12f), 0.0f, 1.0f);
            }
        } else if (t < 0.0f) {
            const float tmp0 = a + de;
            const float tmp1 = b + df;
            if (tmp1 > tmp0) {
                const float numer = tmp1 - tmp0;
                const float denom = a - 2.0f * b + c;
                t = std::clamp(numer / std::max(denom, 1e-12f), 0.0f, 1.0f);
                s = 1.0f - t;
            } else {
                t = 0.0f;
                s = std::clamp(-de / std::max(a, 1e-12f), 0.0f, 1.0f);
            }
        } else {
            const float tmp0 = b + de - df;
            const float tmp1 = a - 2.0f * b + c;
            s = std::clamp(tmp0 / std::max(tmp1, 1e-12f), 0.0f, 1.0f);
            t = 1.0f - s;
        }
    }

    const Vec3 closest = add3(add3(tri.v0, scale3(e0, s)), scale3(e1, t));
    return length3(sub3(closest, p));
}

bool MeshToVolumeConverter::pointInsideMesh(const Vec3& p) const noexcept {
    if (triangles_.empty()) return false;

    const Vec3 rayDir{1.0f, 0.0f, 0.0f};
    int intersectionCount = 0;

    for (const auto& tri : triangles_) {
        const Vec3 e1 = sub3(tri.v1, tri.v0);
        const Vec3 e2 = sub3(tri.v2, tri.v0);
        const Vec3 h = cross3(rayDir, e2);
        const float a = dot3(e1, h);

        if (std::abs(a) < 1e-10f) continue;

        const float f = 1.0f / a;
        const Vec3 s = sub3(p, tri.v0);
        const float u = f * dot3(s, h);

        if (u < 0.0f || u > 1.0f) continue;

        const Vec3 q = cross3(s, e1);
        const float v = f * dot3(rayDir, q);

        if (v < 0.0f || u + v > 1.0f) continue;

        const float t = f * dot3(e2, q);
        if (t > 1e-10f) {
            ++intersectionCount;
        }
    }

    return (intersectionCount & 1) == 1;
}

bool MeshToVolumeConverter::convertToScalarField(const VolumeAABB& bounds, VolumeScalarField& field) const noexcept {
    if (triangles_.empty() || field.empty()) return false;

    const auto res = field.resolution;
    const auto extent = bounds.extent();
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    const float voxelDiag = std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z)
        * std::sqrt(invW * invW + invH * invH + invD * invD) * 0.5f;

    for (int z = 0; z < res.depth; ++z) {
        for (int y = 0; y < res.height; ++y) {
            for (int x = 0; x < res.width; ++x) {
                const Vec3 worldPos{
                    bounds.min.x + (static_cast<float>(x) + 0.5f) * invW * extent.x,
                    bounds.min.y + (static_cast<float>(y) + 0.5f) * invH * extent.y,
                    bounds.min.z + (static_cast<float>(z) + 0.5f) * invD * extent.z,
                };

                float minDist = std::numeric_limits<float>::max();
                for (const auto& tri : triangles_) {
                    minDist = std::min(minDist, pointToTriangleDistance(worldPos, tri));
                }

                const bool inside = settings_.fillInterior && pointInsideMesh(worldPos);
                const float signedDist = inside ? -minDist : minDist;
                field.at(x, y, z) = signedDistanceToVolumeDensity(signedDist, settings_);
            }
        }
    }

    return true;
}

float signedDistanceToVolumeDensity(float signedDist, const MeshToVolumeSettings& settings) noexcept {
    if (signedDist > settings.surfaceThickness) return 0.0f;

    if (signedDist <= 0.0f) {
        const float interior = settings.interiorDensity;
        const float falloff = std::clamp(-signedDist / settings.surfaceFalloff, 0.0f, 1.0f);
        return interior * falloff;
    }

    const float falloff = std::clamp(1.0f - signedDist / settings.surfaceThickness, 0.0f, 1.0f);
    return falloff;
}

} // namespace ArtifactCore::RayTrace