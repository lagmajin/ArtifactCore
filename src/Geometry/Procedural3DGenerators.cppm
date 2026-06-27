module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

module Procedural3DGenerators;

namespace ArtifactCore {

namespace {

int qualityScale(Procedural3DQuality quality)
{
    switch (quality) {
    case Procedural3DQuality::Draft:
        return 1;
    case Procedural3DQuality::Full:
        return 4;
    case Procedural3DQuality::Preview:
    default:
        return 2;
    }
}

std::array<float, 3> cross(const std::array<float, 3>& a, const std::array<float, 3>& b)
{
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

float dot(const std::array<float, 3>& a, const std::array<float, 3>& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

std::array<float, 3> normalized(const std::array<float, 3>& value,
                                const std::array<float, 3>& fallback)
{
    const float lengthSquared = dot(value, value);
    if (lengthSquared <= 1.0e-10f) {
        return fallback;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        value[0] * inverseLength,
        value[1] * inverseLength,
        value[2] * inverseLength
    };
}

float sampleHeightMap(const TerrainSettings& settings, float u, float v)
{
    const int width = settings.heightSampleWidth;
    const int height = settings.heightSampleHeight;
    const std::size_t required =
        width > 0 && height > 0
        ? static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
        : 0u;
    if (required == 0u || settings.heightSamples.size() < required) {
        return 0.5f;
    }

    const float x = std::clamp(u, 0.0f, 1.0f) * static_cast<float>(width - 1);
    const float y = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(height - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(width - 1, x0 + 1);
    const int y1 = std::min(height - 1, y0 + 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](int px, int py) {
        return settings.heightSamples[
            static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(px)];
    };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return std::clamp(top + (bottom - top) * ty, 0.0f, 1.0f);
}

} // namespace

int Procedural3DGenerators::clampGridCount(int value, int minimum, int maximum)
{
    return std::clamp(value, minimum, maximum);
}

float Procedural3DGenerators::clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Procedural3DGenerators::safeSqrt(float value)
{
    return std::sqrt(std::max(0.0f, value));
}

Procedural3DBounds Procedural3DGenerators::computeBounds(const Procedural3DMeshData& mesh)
{
    Procedural3DBounds bounds {};
    if (mesh.vertices.empty()) {
        return bounds;
    }

    bounds.minX = bounds.maxX = mesh.vertices.front().px;
    bounds.minY = bounds.maxY = mesh.vertices.front().py;
    bounds.minZ = bounds.maxZ = mesh.vertices.front().pz;

    for (const auto& v : mesh.vertices) {
        bounds.minX = std::min(bounds.minX, v.px);
        bounds.minY = std::min(bounds.minY, v.py);
        bounds.minZ = std::min(bounds.minZ, v.pz);
        bounds.maxX = std::max(bounds.maxX, v.px);
        bounds.maxY = std::max(bounds.maxY, v.py);
        bounds.maxZ = std::max(bounds.maxZ, v.pz);
    }

    return bounds;
}

void Procedural3DGenerators::computeTerrainNormal(Procedural3DMeshData& mesh, int columns, int rows)
{
    const int vertCount = static_cast<int>(mesh.vertices.size());
    if (vertCount <= 0 || columns <= 0 || rows <= 0) {
        return;
    }

    auto idxAt = [columns](int x, int y) -> std::size_t {
        return static_cast<std::size_t>(y * (columns + 1) + x);
    };

    for (int y = 0; y <= rows; ++y) {
        for (int x = 0; x <= columns; ++x) {
            const std::size_t i = idxAt(x, y);
            const int xl = std::max(0, x - 1);
            const int xr = std::min(columns, x + 1);
            const int yd = std::max(0, y - 1);
            const int yu = std::min(rows, y + 1);

            const float hL = mesh.vertices[idxAt(xl, y)].pz;
            const float hR = mesh.vertices[idxAt(xr, y)].pz;
            const float hD = mesh.vertices[idxAt(x, yd)].pz;
            const float hU = mesh.vertices[idxAt(x, yu)].pz;
            const float xSpan = std::max(
                1.0e-6f,
                mesh.vertices[idxAt(xr, y)].px - mesh.vertices[idxAt(xl, y)].px);
            const float ySpan = std::max(
                1.0e-6f,
                mesh.vertices[idxAt(x, yu)].py - mesh.vertices[idxAt(x, yd)].py);
            const float dzdx = (hR - hL) / xSpan;
            const float dzdy = (hU - hD) / ySpan;
            const float nx = -dzdx;
            const float ny = -dzdy;
            const float len = safeSqrt(nx * nx + ny * ny + 1.0f);
            mesh.vertices[i].nx = nx / len;
            mesh.vertices[i].ny = ny / len;
            mesh.vertices[i].nz = 1.0f / len;
        }
    }
}

Procedural3DMeshData Procedural3DGenerators::generateTerrain(const TerrainSettings& settings, float timeSeconds)
{
    Procedural3DMeshData mesh;
    const int scale = qualityScale(settings.quality);
    const int columns = clampGridCount(settings.columns * scale / 2, 1, 1024);
    const int rows = clampGridCount(settings.rows * scale / 2, 1, 1024);
    const float sizeX = std::max(0.001f, settings.sizeX);
    const float sizeY = std::max(0.001f, settings.sizeY);
    const float height = std::max(0.0f, settings.height);
    const float noiseScale = std::max(0.0001f, settings.noiseScale);
    const float amplitude = std::max(0.0f, settings.noiseAmplitude);
    const int octaves = clampGridCount(settings.noiseOctaves, 1, 12);

    mesh.vertices.reserve(static_cast<std::size_t>((columns + 1) * (rows + 1)));
    mesh.indices.reserve(static_cast<std::size_t>(columns * rows * 6));

    const float invColumns = 1.0f / static_cast<float>(columns);
    const float invRows = 1.0f / static_cast<float>(rows);

    for (int y = 0; y <= rows; ++y) {
        const float v = static_cast<float>(y) * invRows;
        const float py = (v - 0.5f) * sizeY;
        for (int x = 0; x <= columns; ++x) {
            const float u = static_cast<float>(x) * invColumns;
            const float px = (u - 0.5f) * sizeX;
            const float seedOffsetX = static_cast<float>((settings.seed * 1664525u + 1013904223u) & 0xffffu) / 4096.0f;
            const float seedOffsetY = static_cast<float>((settings.seed * 22695477u + 1u) & 0xffffu) / 4096.0f;
            const float sampleX = px * noiseScale + seedOffsetX;
            const float sampleY = py * noiseScale + seedOffsetY;
            const bool hasHeightMap =
                settings.heightSource == TerrainHeightSource::ImageLuminance &&
                settings.heightSampleWidth > 0 &&
                settings.heightSampleHeight > 0 &&
                !settings.heightSamples.empty();
            const float proceduralNoise = NoiseGenerator::fractal(
                sampleX,
                sampleY,
                timeSeconds * settings.noiseEvolution,
                octaves,
                0.5f,
                2.0f) * 0.5f + 0.5f;
            const float n = hasHeightMap
                ? sampleHeightMap(settings, u, v)
                : proceduralNoise;
            const float sourceGain =
                settings.heightSource == TerrainHeightSource::AudioAmplitude
                ? (settings.audioAvailable
                       ? std::clamp(settings.audioAmplitude, 0.0f, 1.0f)
                       : 1.0f)
                : 1.0f;
            const float z =
                (n * 2.0f - 1.0f) * amplitude * height * sourceGain;
            float textureU = u;
            float textureV = v;
            if (settings.uvMode == TerrainUvMode::Polar) {
                const float centeredU = u - 0.5f;
                const float centeredV = v - 0.5f;
                textureU = std::atan2(centeredV, centeredU) /
                               6.28318530718f +
                           0.5f;
                textureV = std::clamp(
                    std::sqrt(centeredU * centeredU + centeredV * centeredV) *
                        1.41421356237f,
                    0.0f,
                    1.0f);
            }
            mesh.vertices.push_back(
                {px, py, z, 0.0f, 0.0f, 1.0f, textureU, textureV});
        }
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(y * (columns + 1) + x);
            const std::uint32_t i1 = i0 + 1u;
            const std::uint32_t i2 = static_cast<std::uint32_t>((y + 1) * (columns + 1) + x);
            const std::uint32_t i3 = i2 + 1u;
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    computeTerrainNormal(mesh, columns, rows);
    return mesh;
}

Procedural3DMeshData Procedural3DGenerators::generatePathTube(const PathTubeSettings& settings, float timeSeconds)
{
    Procedural3DMeshData mesh;
    const int scale = qualityScale(settings.quality);
    const int pathSamples = clampGridCount(settings.pathSamples * scale / 2, 2, 2048);
    const bool ribbon = settings.profile == ProceduralPathProfile::Ribbon;
    const int sides = ribbon ? 2 : clampGridCount(settings.sides * scale / 2, 3, 64);
    const float radius = std::max(0.0f, settings.radius);
    const float taperStart = std::max(0.0f, settings.taperStart);
    const float taperEnd = std::max(0.0f, settings.taperEnd);
    const float pathScale = std::max(0.0001f, settings.pathScale);
    const float noiseScale = std::max(0.0001f, settings.noiseScale);
    const float noiseAmp = std::max(0.0f, settings.noiseAmplitude);
    const float twist = settings.twist + timeSeconds * 0.15f;
    const float repeatCount = std::max(0.001f, settings.repeatCount);

    if (radius <= 0.0f || pathScale <= 0.0f) {
        return mesh;
    }

    mesh.vertices.reserve(static_cast<std::size_t>(pathSamples * sides));
    mesh.indices.reserve(static_cast<std::size_t>((pathSamples - 1) * sides * 6));

    auto pathPoint = [&](float t) -> std::array<float, 3> {
        const float pathT = t + settings.pathOffset;
        if (settings.pathSource == ProceduralPathSource::ControlPoints &&
            settings.pathPoints.size() >= 2u) {
            const std::size_t pointCount = settings.pathPoints.size();
            const float repeatedT = settings.pathClosed
                ? pathT * repeatCount
                : pathT;
            const float normalizedT = settings.pathClosed
                ? repeatedT - std::floor(repeatedT)
                : std::clamp(repeatedT, 0.0f, 1.0f);
            const float segmentPosition = normalizedT * static_cast<float>(
                settings.pathClosed ? pointCount : pointCount - 1u);
            const std::size_t first = std::min(
                pointCount - 1u,
                static_cast<std::size_t>(std::floor(segmentPosition)));
            const std::size_t second = settings.pathClosed
                ? (first + 1u) % pointCount
                : std::min(pointCount - 1u, first + 1u);
            const float amount = segmentPosition - std::floor(segmentPosition);
            const auto& a = settings.pathPoints[first];
            const auto& b = settings.pathPoints[second];
            const float seedPhase =
                static_cast<float>(settings.seed & 0xffffu) * 0.001953125f;
            const float wobble = NoiseGenerator::perlin(
                normalizedT * 3.0f * noiseScale + seedPhase,
                timeSeconds * 0.35f,
                seedPhase * 0.37f) * noiseAmp;
            return {
                (a[0] + (b[0] - a[0]) * amount) * pathScale + wobble * 0.3f,
                (a[1] + (b[1] - a[1]) * amount) * pathScale,
                (a[2] + (b[2] - a[2]) * amount) * pathScale + wobble
            };
        }
        const float angle = pathT * repeatCount * 6.28318530718f;
        const float x = std::cos(angle) * 0.55f;
        const float y = std::sin(angle * 0.73f) * 0.35f;
        const float z = (t - 0.5f) * 1.2f;
        const float seedPhase = static_cast<float>(settings.seed & 0xffffu) * 0.001953125f;
        const float wobble = NoiseGenerator::perlin(pathT * 3.0f * noiseScale + seedPhase,
                                                    timeSeconds * 0.35f,
                                                    seedPhase * 0.37f) * noiseAmp;
        return {x * pathScale + wobble * 0.3f, y * pathScale, z * pathScale + wobble};
    };

    auto tangentAt = [&](float t) -> std::array<float, 3> {
        const float dt = 0.001f;
        const float t0 = std::max(0.0f, t - dt);
        const float t1 = std::min(1.0f, t + dt);
        const auto p0 = pathPoint(t0);
        const auto p1 = pathPoint(t1);
        return {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
    };

    std::array<float, 3> previousTangent {0.0f, 0.0f, 1.0f};
    std::array<float, 3> previousNormal {0.0f, 1.0f, 0.0f};

    for (int i = 0; i < pathSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pathSamples - 1);
        const auto p = pathPoint(t);
        const auto tan = tangentAt(t);
        const auto T = normalized(tan, previousTangent);
        const float tangentAlignment = dot(previousTangent, T);
        std::array<float, 3> N = previousNormal;
        if (i == 0 || tangentAlignment < -0.999f) {
            const std::array<float, 3> reference =
                std::abs(T[2]) > 0.92f ? std::array<float, 3>{0.0f, 1.0f, 0.0f}
                                      : std::array<float, 3>{0.0f, 0.0f, 1.0f};
            N = normalized(cross(reference, T), previousNormal);
        } else {
            const float alongTangent = dot(N, T);
            N = normalized({
                N[0] - T[0] * alongTangent,
                N[1] - T[1] * alongTangent,
                N[2] - T[2] * alongTangent
            }, previousNormal);
        }
        auto B = normalized(cross(T, N), {1.0f, 0.0f, 0.0f});
        N = normalized(cross(B, T), N);

        previousTangent = T;
        previousNormal = N;

        const float profileRadius = radius * (taperStart + (taperEnd - taperStart) * t);
        const float twistAngle = twist * t;
        const float ct = std::cos(twistAngle);
        const float st = std::sin(twistAngle);

        for (int s = 0; s < sides; ++s) {
            const float sideT = ribbon
                ? static_cast<float>(s)
                : static_cast<float>(s) / static_cast<float>(sides);
            const float ang = ribbon ? (s == 0 ? 0.0f : 3.14159265359f)
                                     : sideT * 6.28318530718f;
            const float ca = std::cos(ang);
            const float sa = std::sin(ang);
            const float rotatedB[3] = {
                B[0] * ct + N[0] * st,
                B[1] * ct + N[1] * st,
                B[2] * ct + N[2] * st
            };
            const float rotatedN[3] = {
                N[0] * ct - B[0] * st,
                N[1] * ct - B[1] * st,
                N[2] * ct - B[2] * st
            };
            const float rx = rotatedB[0] * ca + rotatedN[0] * sa;
            const float ry = rotatedB[1] * ca + rotatedN[1] * sa;
            const float rz = rotatedB[2] * ca + rotatedN[2] * sa;
            const float normalX = ribbon ? rotatedN[0] : rx;
            const float normalY = ribbon ? rotatedN[1] : ry;
            const float normalZ = ribbon ? rotatedN[2] : rz;

            mesh.vertices.push_back({
                p[0] + rx * profileRadius,
                p[1] + ry * profileRadius,
                p[2] + rz * profileRadius,
                normalX,
                normalY,
                normalZ,
                sideT,
                t
            });
        }
    }

    for (int i = 0; i < pathSamples - 1; ++i) {
        const int edgeCount = ribbon ? 1 : sides;
        for (int s = 0; s < edgeCount; ++s) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(i * sides + s);
            const std::uint32_t i1 = static_cast<std::uint32_t>(i * sides + (ribbon ? s + 1 : (s + 1) % sides));
            const std::uint32_t i2 = static_cast<std::uint32_t>((i + 1) * sides + s);
            const std::uint32_t i3 = static_cast<std::uint32_t>((i + 1) * sides + (ribbon ? s + 1 : (s + 1) % sides));
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    return mesh;
}

Procedural3DResult Procedural3DGenerators::generateTerrainResult(const TerrainSettings& settings,
                                                                float timeSeconds,
                                                                Procedural3DShading shading)
{
    Procedural3DResult result;
    result.kind = Procedural3DKind::Terrain;
    result.shading = shading;
    result.quality = settings.quality;
    result.mesh = generateTerrain(settings, timeSeconds);
    result.valid = !result.mesh.vertices.empty();
    return result;
}

Procedural3DResult Procedural3DGenerators::generatePathTubeResult(const PathTubeSettings& settings,
                                                                 float timeSeconds,
                                                                 Procedural3DShading shading)
{
    Procedural3DResult result;
    result.kind = Procedural3DKind::PathTube;
    result.shading = shading;
    result.quality = settings.quality;
    result.mesh = generatePathTube(settings, timeSeconds);
    result.valid = !result.mesh.vertices.empty();
    return result;
}

TerrainSettings Procedural3DGenerators::makeTerrainPreset(std::uint32_t seed, Procedural3DQuality quality)
{
    TerrainSettings settings;
    settings.seed = seed;
    settings.quality = quality;
    settings.columns = (quality == Procedural3DQuality::Draft) ? 24 : (quality == Procedural3DQuality::Preview ? 64 : 128);
    settings.rows = settings.columns;
    settings.height = 2.0f;
    settings.noiseScale = 0.9f;
    settings.noiseAmplitude = 1.0f;
    settings.noiseOctaves = (quality == Procedural3DQuality::Full) ? 6 : 4;
    settings.noiseEvolution = 0.25f;
    return settings;
}

PathTubeSettings Procedural3DGenerators::makePathTubePreset(std::uint32_t seed, Procedural3DQuality quality)
{
    PathTubeSettings settings;
    settings.seed = seed;
    settings.quality = quality;
    settings.pathSamples = (quality == Procedural3DQuality::Draft) ? 48 : (quality == Procedural3DQuality::Preview ? 128 : 256);
    settings.sides = (quality == Procedural3DQuality::Draft) ? 6 : 8;
    settings.radius = 0.15f;
    settings.taperStart = 1.0f;
    settings.taperEnd = 0.6f;
    settings.twist = 1.25f;
    settings.pathOffset = 0.0f;
    settings.repeatCount = 1.0f;
    settings.pathScale = 1.0f;
    settings.noiseScale = 1.0f;
    settings.noiseAmplitude = 0.08f;
    return settings;
}

Procedural3DPreviewInfo Procedural3DGenerators::previewInfo(const Procedural3DResult& result)
{
    Procedural3DPreviewInfo info;
    info.kind = result.kind;
    info.shading = result.shading;
    info.vertexCount = static_cast<std::uint32_t>(result.mesh.vertices.size());
    info.indexCount = static_cast<std::uint32_t>(result.mesh.indices.size());
    info.bounds = computeBounds(result.mesh);
    info.quality = result.quality;
    return info;
}

} // namespace ArtifactCore
