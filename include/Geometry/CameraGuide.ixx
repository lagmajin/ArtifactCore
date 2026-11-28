module;
class tst_QList;
#include <QVector>
#include <QVector3D>
#include <utility>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

export module Geometry.CameraGuide;

export namespace ArtifactCore {

struct CameraBodyGuide {
    // The lens faces local -Z and its front is at the optical origin.
    using Ring = std::array<QVector3D, 8>;
    std::array<Ring, 4> shell;
    std::array<Ring, 5> lens;
    std::array<QVector3D, 3> upMark;
};

inline const CameraBodyGuide& nukeStyleCameraBodyGuide()
{
    const auto shellRing = [](float x, float y, float z) {
        return CameraBodyGuide::Ring{{
            {-0.68f*x, -y, z}, {0.68f*x, -y, z},
            {x, -0.62f*y, z}, {x, 0.62f*y, z},
            {0.68f*x, y, z}, {-0.68f*x, y, z},
            {-x, 0.62f*y, z}, {-x, -0.62f*y, z},
        }};
    };
    const auto lensRing = [](float radius, float z) {
        const float d = radius * 0.70710678f;
        return CameraBodyGuide::Ring{{
            {radius, 0, z}, {d, d, z}, {0, radius, z}, {-d, d, z},
            {-radius, 0, z}, {-d, -d, z}, {0, -radius, z}, {d, -d, z},
        }};
    };
    static const CameraBodyGuide guide{
        {shellRing(0.20f, 0.14f, 0.43f), shellRing(0.24f, 0.17f, 0.37f),
         shellRing(0.24f, 0.17f, 0.23f), shellRing(0.17f, 0.12f, 0.16f)},
        {lensRing(0.10f, 0.16f), lensRing(0.10f, 0.09f), lensRing(0.13f, 0.07f),
         lensRing(0.13f, 0.015f), lensRing(0.10f, 0.0f)},
        {{{-0.105f, 0.173f, 0.34f}, {0, 0.173f, 0.25f}, {0.105f, 0.173f, 0.34f}}},
    };
    return guide;
}

struct CameraGuidePrimitive {
    std::vector<QVector3D> vertices;
    std::vector<std::uint32_t> lineIndices;
    std::vector<std::uint32_t> triangleIndices;
};

inline CameraGuidePrimitive makeNukeStyleCameraGuidePrimitive(
    float bodyWidth = 0.55f,
    float bodyHeight = 0.36f,
    float bodyDepth = 0.28f,
    float frustumNearWidth = 0.42f,
    float frustumNearHeight = 0.28f,
    float frustumFarWidth = 0.95f,
    float frustumFarHeight = 0.64f,
    float frustumNearZ = -0.36f,
    float frustumFarZ = -1.45f)
{
    CameraGuidePrimitive guide;

    const float halfBodyW = bodyWidth * 0.5f;
    const float halfBodyH = bodyHeight * 0.5f;
    const float bodyFrontZ = 0.0f;
    const float bodyBackZ = -std::max(bodyDepth, 0.01f);

    const float halfNearW = frustumNearWidth * 0.5f;
    const float halfNearH = frustumNearHeight * 0.5f;
    const float halfFarW = frustumFarWidth * 0.5f;
    const float halfFarH = frustumFarHeight * 0.5f;

    guide.vertices = {
        // Body box
        {-halfBodyW, -halfBodyH, bodyFrontZ}, // 0
        { halfBodyW, -halfBodyH, bodyFrontZ}, // 1
        { halfBodyW,  halfBodyH, bodyFrontZ}, // 2
        {-halfBodyW,  halfBodyH, bodyFrontZ}, // 3
        {-halfBodyW, -halfBodyH, bodyBackZ},  // 4
        { halfBodyW, -halfBodyH, bodyBackZ},  // 5
        { halfBodyW,  halfBodyH, bodyBackZ},  // 6
        {-halfBodyW,  halfBodyH, bodyBackZ},  // 7

        // Frustum near plane
        {-halfNearW, -halfNearH, frustumNearZ}, // 8
        { halfNearW, -halfNearH, frustumNearZ}, // 9
        { halfNearW,  halfNearH, frustumNearZ}, // 10
        {-halfNearW,  halfNearH, frustumNearZ}, // 11

        // Frustum far plane
        {-halfFarW, -halfFarH, frustumFarZ}, // 12
        { halfFarW, -halfFarH, frustumFarZ}, // 13
        { halfFarW,  halfFarH, frustumFarZ}, // 14
        {-halfFarW,  halfFarH, frustumFarZ}, // 15
    };

    guide.lineIndices = {
        // Body box edges
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,

        // Frustum near plane
        8, 9, 9, 10, 10, 11, 11, 8,

        // Frustum far plane
        12, 13, 13, 14, 14, 15, 15, 12,

        // Connect body to near plane
        1, 9, 2, 10, 3, 11,

        // Connect near plane to far plane
        8, 12, 9, 13, 10, 14, 11, 15,
    };

    guide.triangleIndices = {
        // Body box
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        1, 2, 6, 1, 6, 5,
        2, 3, 7, 2, 7, 6,
        3, 0, 4, 3, 4, 7,
    };

    return guide;
}

} // namespace ArtifactCore
