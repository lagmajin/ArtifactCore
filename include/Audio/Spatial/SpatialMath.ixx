module;
#include <cmath>
#include <algorithm>

export module Audio.Spatial.Math;

import Audio.Spatial.Params;

export namespace ArtifactCore {
namespace Audio {
namespace Spatial {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

inline float fastRsqrt(float v) {
    if (!(v > 1e-12f) || !std::isfinite(v)) return 0.0f;
    float r = 1.0f / std::sqrt(v);
    r = r * (1.5f - 0.5f * v * r * r);
    return r;
}

inline float distanceAttenuation(float distance, const SpatialParams& p) {
    if (distance <= p.minDistance) return 1.0f;
    if (distance >= p.maxDistance) {
        if (p.model == DistanceModel::Linear) return 0.0f;
        float d = p.maxDistance;
        if (p.model == DistanceModel::Inverse) {
            return p.minDistance / (p.minDistance + p.rolloff * (d - p.minDistance));
        }
        return std::pow(std::max(d / p.minDistance, 1.0f), -p.rolloff);
    }
    switch (p.model) {
        case DistanceModel::Linear: {
            float t = (distance - p.minDistance) / (p.maxDistance - p.minDistance);
            t = std::clamp(t, 0.0f, 1.0f);
            return 1.0f - p.rolloff * t;
        }
        case DistanceModel::Inverse:
            return p.minDistance / (p.minDistance + p.rolloff * (distance - p.minDistance));
        case DistanceModel::Exponential:
            return std::pow(std::max(distance / p.minDistance, 1.0f), -p.rolloff);
    }
    return 1.0f;
}

inline float coneGain(float angleDeg, const SpatialParams& p) {
    if (p.coneInnerAngle >= 360.0f) return 1.0f;
    if (angleDeg <= p.coneInnerAngle * 0.5f) return 1.0f;
    if (angleDeg >= p.coneOuterAngle * 0.5f) return p.coneOuterGain;
    float t = (angleDeg - p.coneInnerAngle * 0.5f) / (p.coneOuterAngle * 0.5f - p.coneInnerAngle * 0.5f);
    t = std::clamp(t, 0.0f, 1.0f);
    float s = t * t * (3.0f - 2.0f * t);
    return 1.0f + s * (p.coneOuterGain - 1.0f);
}

inline Vec3 vecSub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline Quat quatConjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }

inline Vec3 quatRotate(Quat q, Vec3 v) {
    float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
    float tx = 2.0f * (qy * v.z - qz * v.y);
    float ty = 2.0f * (qz * v.x - qx * v.z);
    float tz = 2.0f * (qx * v.y - qy * v.x);
    return {
        v.x + qw * tx + qy * tz - qz * ty,
        v.y + qw * ty + qz * tx - qx * tz,
        v.z + qw * tz + qx * ty - qy * tx
    };
}

inline void toSpherical(Vec3 local, float& azimuth, float& elevation) {
    float len2 = local.x * local.x + local.y * local.y + local.z * local.z;
    if (len2 < 1e-12f) { azimuth = 0.0f; elevation = 0.0f; return; }
    float len = std::sqrt(len2);
    azimuth = std::atan2(local.x, -local.z) * 57.29577951308232f;
    elevation = std::asin(std::clamp(local.y / len, -1.0f, 1.0f)) * 57.29577951308232f;
}

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
