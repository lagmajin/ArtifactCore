module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include <QFont>
#include <QFontMetrics>
#include <QFontInfo>
#include <QCryptographicHash>
#include <QByteArray>
#include <QChar>
#include <QGlyphRun>
#include <QList>
#include <QPainterPath>
#include <QRawFont>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QTextLayout>
#include <QTextOption>

module Procedural3DGenerators;

import Core.Parallel;
import Core.ArtifactArray;
import Core.ArtifactSet;
import Font.FreeFont;
import Mesh;
import Geometry.ShapeExtrude;

namespace ArtifactCore {

namespace {

struct QStringHasher {
    std::size_t operator()(const QString& value) const noexcept
    {
        return static_cast<std::size_t>(qHash(value));
    }
};

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

    Parallel::ForPixels(0, rows + 1, columns + 1, rows + 1, [&](int y) {
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
    });
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

    const int vertexColumns = columns + 1;
    const int vertexRows = rows + 1;
    mesh.vertices.resize(static_cast<std::size_t>(vertexColumns * vertexRows));
    mesh.indices.resize(static_cast<std::size_t>(columns * rows * 6));

    const float invColumns = 1.0f / static_cast<float>(columns);
    const float invRows = 1.0f / static_cast<float>(rows);
    const float seedOffsetX = static_cast<float>((settings.seed * 1664525u + 1013904223u) & 0xffffu) / 4096.0f;
    const float seedOffsetY = static_cast<float>((settings.seed * 22695477u + 1u) & 0xffffu) / 4096.0f;
    const bool hasHeightMap =
        settings.heightSource == TerrainHeightSource::ImageLuminance &&
        settings.heightSampleWidth > 0 &&
        settings.heightSampleHeight > 0 &&
        !settings.heightSamples.empty();
    const float sourceGain =
        settings.heightSource == TerrainHeightSource::AudioAmplitude
        ? (settings.audioAvailable
               ? std::clamp(settings.audioAmplitude, 0.0f, 1.0f)
               : 1.0f)
        : 1.0f;

    ArtifactCore::Parallel::ForPixels(
        0, vertexRows, vertexColumns, vertexRows, [&](int y) {
        const float v = static_cast<float>(y) * invRows;
        const float py = (v - 0.5f) * sizeY;
        for (int x = 0; x <= columns; ++x) {
            const float u = static_cast<float>(x) * invColumns;
            const float px = (u - 0.5f) * sizeX;
            const float sampleX = px * noiseScale + seedOffsetX;
            const float sampleY = py * noiseScale + seedOffsetY;
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
            mesh.vertices[static_cast<std::size_t>(y * vertexColumns + x)] =
                {px, py, z, 0.0f, 0.0f, 1.0f, textureU, textureV};
        }
    });

    ArtifactCore::Parallel::ForPixels(0, rows, columns, rows, [&](int y) {
        for (int x = 0; x < columns; ++x) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(y * (columns + 1) + x);
            const std::uint32_t i1 = i0 + 1u;
            const std::uint32_t i2 = static_cast<std::uint32_t>((y + 1) * (columns + 1) + x);
            const std::uint32_t i3 = i2 + 1u;
            const std::size_t base = static_cast<std::size_t>(y * columns + x) * 6u;
            mesh.indices[base + 0] = i0;
            mesh.indices[base + 1] = i2;
            mesh.indices[base + 2] = i1;
            mesh.indices[base + 3] = i1;
            mesh.indices[base + 4] = i2;
            mesh.indices[base + 5] = i3;
        }
    });

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
    // 3D-Stroke style trim: emit only the sub-range [trimStart, trimEnd].
    const float trimStart = std::clamp(settings.trimStart, 0.0f, 1.0f);
    const float trimEnd = std::clamp(settings.trimEnd, 0.0f, 1.0f);

    if (radius <= 0.0f || pathScale <= 0.0f) {
        return mesh;
    }
    if (trimEnd <= trimStart + 1.0e-6f) {
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
        const float te = trimStart + t * (trimEnd - trimStart);
        const auto p = pathPoint(te);
        const auto tan = tangentAt(te);
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

        const float profileRadius = radius * (taperStart + (taperEnd - taperStart) * te);
        const float twistAngle = twist * te;
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
                te
            });
        }
    }

    const int edgeCount = ribbon ? 1 : sides;
    mesh.indices.resize(static_cast<std::size_t>((pathSamples - 1) * edgeCount * 6));
    ArtifactCore::Parallel::ForPixels(
        0, pathSamples - 1, edgeCount, pathSamples - 1, [&](int i) {
        for (int s = 0; s < edgeCount; ++s) {
            const std::uint32_t i0 = static_cast<std::uint32_t>(i * sides + s);
            const std::uint32_t i1 = static_cast<std::uint32_t>(i * sides + (ribbon ? s + 1 : (s + 1) % sides));
            const std::uint32_t i2 = static_cast<std::uint32_t>((i + 1) * sides + s);
            const std::uint32_t i3 = static_cast<std::uint32_t>((i + 1) * sides + (ribbon ? s + 1 : (s + 1) % sides));
            const std::size_t base = static_cast<std::size_t>(i * edgeCount + s) * 6u;
            mesh.indices[base + 0] = i0;
            mesh.indices[base + 1] = i2;
            mesh.indices[base + 2] = i1;
            mesh.indices[base + 3] = i1;
            mesh.indices[base + 4] = i2;
            mesh.indices[base + 5] = i3;
        }
    });

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
    if (!result.valid) {
        result.status = Procedural3DGenerationStatus::InvalidGeometry;
        result.diagnosticMessage = QStringLiteral("Terrain generation produced no mesh.");
    }
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
    if (!result.valid) {
        result.status = Procedural3DGenerationStatus::InvalidGeometry;
        result.diagnosticMessage = QStringLiteral("Path tube generation produced no mesh.");
    }
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

Procedural3DMeshData Procedural3DGenerators::generateTextExtrude(
    const TextExtrudeSettings& settings,
    float /*timeSeconds*/,
    TextExtrudeDiagnostics* diagnostics)
{
    Procedural3DMeshData meshData;
    if (diagnostics != nullptr) *diagnostics = {};
    const auto fail = [&](Procedural3DGenerationStatus status, const QString& message) {
        if (diagnostics != nullptr) {
            diagnostics->status = status;
            diagnostics->message = message;
        }
        return Procedural3DMeshData{};
    };
    if (settings.text.trimmed().isEmpty()) {
        return fail(Procedural3DGenerationStatus::EmptyInput,
                    QStringLiteral("Text is empty or contains only whitespace."));
    }
    constexpr std::uint32_t hardGlyphLimit = 16384u;
    constexpr qsizetype hardTextCodeUnitLimit = 65536;
    constexpr qsizetype hardLineCountLimit = 4096;
    const std::uint64_t glyphLimit = std::min<std::uint64_t>(
        settings.maxGlyphCount, hardGlyphLimit);
    if (glyphLimit == 0u
        || settings.text.size() > hardTextCodeUnitLimit
        || static_cast<std::uint64_t>(settings.text.size()) > glyphLimit * 4u) {
        return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                    QStringLiteral("Text length exceeds the configured shaping budget."));
    }
    qsizetype lineCount = 1;
    for (const QChar character : settings.text) {
        if (character == QLatin1Char('\n')
            && ++lineCount > hardLineCountLimit) {
            return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                        QStringLiteral("Text line count exceeds the layout budget."));
        }
    }
    if (!std::isfinite(settings.depth) || settings.depth <= 0.0f
        || !std::isfinite(settings.bevelWidth)
        || !std::isfinite(settings.letterSpacing)
        || !std::isfinite(settings.lineSpacing) || settings.lineSpacing <= 0.0f
        || static_cast<std::uint32_t>(settings.alignment) > 2u
        || settings.fontSize <= 0) {
        return fail(Procedural3DGenerationStatus::InvalidSettings,
                    QStringLiteral("Font size, depth, spacing, and bevel settings must be finite and valid."));
    }
    QFont font(settings.fontFamily);
    font.setPointSize(std::clamp(settings.fontSize, 1, 1000));
    font.setBold(settings.bold);
    font.setItalic(settings.italic);
    font.setLetterSpacing(QFont::AbsoluteSpacing,
                          std::clamp(settings.letterSpacing, -1000.0f, 1000.0f));
    if (diagnostics != nullptr) {
        diagnostics->resolvedFontFamily = QFontInfo(font).family().left(256);
    }
    QTextLayout layout(settings.text, font);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(textOption);
    const QFontMetricsF metrics(font);
    const qreal lineSpacing = metrics.lineSpacing();
    layout.beginLayout();
    qreal lineY = 0.0;
    ArtifactArray<QTextLine> layoutLines;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(std::numeric_limits<qreal>::max());
        line.setPosition(QPointF(0.0, lineY));
        lineY += lineSpacing * std::clamp(settings.lineSpacing, 0.25f, 4.0f);
        layoutLines.append(line);
    }
    qreal blockWidth = 0.0;
    for (const QTextLine& line : layoutLines) {
        blockWidth = std::max(blockWidth, line.naturalTextWidth());
    }
    for (QTextLine& line : layoutLines) {
        qreal offsetX = 0.0;
        if (settings.alignment == TextExtrudeAlignment::Center) {
            offsetX = (blockWidth - line.naturalTextWidth()) * 0.5;
        } else if (settings.alignment == TextExtrudeAlignment::Right) {
            offsetX = blockWidth - line.naturalTextWidth();
        }
        const QPointF position = line.position();
        line.setPosition(QPointF(position.x() + offsetX, position.y()));
    }
    layout.endLayout();
    QPainterPath full;
    constexpr qsizetype maxPathElements = 1'000'000;
    std::uint64_t shapedGlyphCount = 0u;
    ArtifactSet<QString, QStringHasher> seenFontFamilies;
    ArtifactSet<QString, QStringHasher> hashedFontKeys;
    bool fontMetadataWithinBounds = true;
    const auto runs = layout.glyphRuns();
    for (const QGlyphRun& run : runs) {
        const QRawFont rawFont = run.rawFont();
        const auto glyphIndexes = run.glyphIndexes();
        const auto glyphPositions = run.positions();
        if (!rawFont.isValid() || glyphIndexes.size() != glyphPositions.size()) {
            continue;
        }
        if (diagnostics != nullptr) {
            const QString fullRunFamily = rawFont.familyName();
            const QString fullRunStyle = rawFont.styleName();
            if (fullRunFamily.size() > 256 || fullRunStyle.size() > 128) {
                fontMetadataWithinBounds = false;
            }
            const QString runFamily = fullRunFamily.left(256);
            if (diagnostics->resolvedFontFamilies.isEmpty()
                && !runFamily.isEmpty()) {
                diagnostics->resolvedFontFamily = runFamily;
            }
            if (!runFamily.isEmpty() && seenFontFamilies.add(runFamily)) {
                if (diagnostics->resolvedFontFamilies.size() < 64) {
                    diagnostics->resolvedFontFamilies.push_back(runFamily);
                } else {
                    fontMetadataWithinBounds = false;
                }
            }
            const QString runStyle = fullRunStyle.left(128);
            const QString fontKey = runFamily + QChar(0x001F) + runStyle;
            if (!runFamily.isEmpty() && hashedFontKeys.add(fontKey)) {
                const auto fontBytes = FontManager::fontFileBytes(runFamily, runStyle);
                if (!fontBytes.has_value() || fontBytes->isEmpty()) {
                    diagnostics->fontIdentityComplete = false;
                } else {
                    const QRawFont fileFont(*fontBytes, rawFont.pixelSize());
                    const QByteArray fileCmap = fileFont.fontTable("cmap");
                    const QByteArray runCmap = rawFont.fontTable("cmap");
                    bool matchingOutlineTableFound = false;
                    for (const char* tableTag : {"glyf", "CFF ", "CFF2"}) {
                        const QByteArray fileOutline = fileFont.fontTable(tableTag);
                        const QByteArray runOutline = rawFont.fontTable(tableTag);
                        if (fileOutline.isEmpty() || runOutline.isEmpty()) continue;
                        matchingOutlineTableFound = fileOutline == runOutline;
                        break;
                    }
                    if (!fileFont.isValid() || fileCmap.isEmpty()
                        || fileCmap != runCmap || !matchingOutlineTableFound) {
                        diagnostics->fontIdentityComplete = false;
                    } else {
                        const QByteArray digest = QCryptographicHash::hash(
                            *fontBytes, QCryptographicHash::Sha256).toHex();
                        if (diagnostics->resolvedFontFingerprints.size() < 64) {
                            diagnostics->resolvedFontFingerprints.push_back(
                                runFamily + QLatin1Char('|') + runStyle + QLatin1Char('|')
                                + QString::fromLatin1(digest));
                        } else {
                            fontMetadataWithinBounds = false;
                        }
                    }
                }
            }
        }
        const std::uint64_t nextGlyphCount = shapedGlyphCount
            + static_cast<std::uint64_t>(glyphIndexes.size());
        shapedGlyphCount = nextGlyphCount;
        if (diagnostics != nullptr) {
            diagnostics->shapedGlyphCount = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(nextGlyphCount,
                    std::numeric_limits<std::uint32_t>::max()));
        }
        if (nextGlyphCount > glyphLimit) {
            return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                        QStringLiteral("Shaped glyph count exceeds the configured mesh limit."));
        }
        if (diagnostics != nullptr) {
            const QList<qsizetype> stringIndexes = run.stringIndexes();
            const QString sourceString = run.sourceString();
            for (qsizetype glyph = 0; glyph < glyphIndexes.size(); ++glyph) {
                if (glyphIndexes[glyph] != 0u) continue;
                ++diagnostics->missingGlyphCount;
                if (diagnostics->missingGlyphLocations.size() >= 128) {
                    diagnostics->missingGlyphLocationsComplete = false;
                    continue;
                }
                if (glyph >= stringIndexes.size() || sourceString.isEmpty()) {
                    diagnostics->missingGlyphLocationsComplete = false;
                    continue;
                }
                const qsizetype stringIndex = stringIndexes[glyph];
                if (stringIndex < 0 || stringIndex >= sourceString.size()) {
                    diagnostics->missingGlyphLocationsComplete = false;
                    continue;
                }
                const QChar firstUnit = sourceString.at(stringIndex);
                uint codePoint = firstUnit.unicode();
                qsizetype characterOffset = stringIndex;
                if (firstUnit.isHighSurrogate()
                    && stringIndex + 1 < sourceString.size()
                    && sourceString.at(stringIndex + 1).isLowSurrogate()) {
                    codePoint = QChar::surrogateToUcs4(
                        firstUnit, sourceString.at(stringIndex + 1));
                } else if (firstUnit.isLowSurrogate() && stringIndex > 0
                           && sourceString.at(stringIndex - 1).isHighSurrogate()) {
                    codePoint = QChar::surrogateToUcs4(
                        sourceString.at(stringIndex - 1), firstUnit);
                    --characterOffset;
                }
                const QString detail = QStringLiteral("UTF-16 offset %1: U+%2")
                    .arg(characterOffset)
                    .arg(static_cast<qulonglong>(codePoint), 4, 16,
                         QLatin1Char('0')).toUpper();
                diagnostics->missingGlyphLocations.push_back(detail);
            }
        }
        for (qsizetype glyphIndex = 0; glyphIndex < glyphIndexes.size(); ++glyphIndex) {
            const QPainterPath glyphPath = rawFont.pathForGlyph(glyphIndexes[glyphIndex]);
            if (glyphPath.isEmpty()) continue;
            QTransform glyphTransform;
            const QPointF glyphPosition = glyphPositions[glyphIndex];
            glyphTransform.translate(glyphPosition.x(), glyphPosition.y());
            full.addPath(glyphTransform.map(glyphPath));
            if (full.elementCount() > maxPathElements) {
                return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                            QStringLiteral("Text outline exceeds the path element budget."));
            }
        }
    }
    // Glyph count alone does not bound the complexity of a font outline.
    // Keep pathological fonts from sending an unbounded path into simplify()
    // and the quadratic contour nesting checks in ShapeExtrude.
    if (full.elementCount() > maxPathElements) {
        return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                    QStringLiteral("Text outline exceeds the path element budget."));
    }
    if (diagnostics != nullptr && !diagnostics->resolvedFontFamilies.isEmpty()
        && !diagnostics->resolvedFontFingerprints.isEmpty()) {
        diagnostics->fontIdentityComplete =
            fontMetadataWithinBounds
            && static_cast<std::size_t>(
                diagnostics->resolvedFontFingerprints.size())
            == hashedFontKeys.size();
    }
    if (full.isEmpty()) {
        return fail(Procedural3DGenerationStatus::NoOutlines,
                    QStringLiteral("The selected font produced no vector outlines."));
    }
    // Text glyph outlines can overlap in Arabic joining forms and in marks or
    // components from CJK/Indic fonts. Normalize the fill before splitting it
    // into contours so overlapping glyph outlines do not become false holes.
    const QPainterPath filledPath = full.simplified();
    if (filledPath.isEmpty()) {
        return fail(Procedural3DGenerationStatus::NoOutlines,
                    QStringLiteral("The shaped text has no fillable vector outlines."));
    }
    if (filledPath.elementCount() > maxPathElements) {
        return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                    QStringLiteral("Normalized text outline exceeds the path element budget."));
    }
    const QRectF bounds = filledPath.boundingRect();
    if (!bounds.isValid() || bounds.isEmpty()) {
        return fail(Procedural3DGenerationStatus::NoOutlines,
                    QStringLiteral("The shaped text has degenerate bounds."));
    }
    // Block center to origin, Y flipped (Qt y-down -> 3D y-up).
    const qreal centerX = bounds.center().x();
    const qreal centerY = bounds.center().y();
    std::vector<std::vector<QPointF>> contours;
    const QList<QPolygonF> subpaths = filledPath.toSubpathPolygons();
    constexpr qsizetype maxContourCount = 4096;
    constexpr std::uint64_t maxFlattenedContourPoints = 250'000u;
    if (subpaths.size() > maxContourCount) {
        return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                    QStringLiteral("Text outline exceeds the contour count budget."));
    }
    contours.reserve(static_cast<std::size_t>(subpaths.size()));
    std::uint64_t flattenedPointCount = 0u;
    for (const QPolygonF& polygon : subpaths) {
        if (polygon.size() < 3) {
            continue;
        }
        flattenedPointCount += static_cast<std::uint64_t>(polygon.size());
        if (flattenedPointCount > maxFlattenedContourPoints) {
            return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                        QStringLiteral("Text outline exceeds the flattened point budget."));
        }
        std::vector<QPointF> contour;
        contour.reserve(static_cast<std::size_t>(polygon.size()));
        for (const QPointF& point : polygon) {
            contour.emplace_back(point.x() - centerX, -(point.y() - centerY));
        }
        contours.push_back(std::move(contour));
    }
    if (contours.empty()) {
        return fail(Procedural3DGenerationStatus::NoOutlines,
                    QStringLiteral("The selected font produced no usable contours."));
    }
    const int requestedBevelSegments = std::clamp(settings.bevelSegments, 1, 8);
    const int effectiveBevelSegments = settings.bevelWidth > 0.0f
        ? std::clamp(requestedBevelSegments * qualityScale(settings.quality), 1, 8)
        : 0;
    constexpr std::uint64_t maxEstimatedMeshVertices = 1'000'000u;
    constexpr std::uint64_t maxEstimatedMeshFaces = 1'000'000u;
    constexpr std::uint64_t maxEstimatedRenderVertices = 1'000'000u;
    // ShapeExtrude allocates two wall vertices and front/back rings for each
    // segment plus the cap-side ring, then duplicates cap vertices for both
    // sides. The margin covers bridge points added while merging nested counters.
    const std::uint64_t estimatedVerticesPerContourPoint =
        static_cast<std::uint64_t>(2 * effectiveBevelSegments + 10);
    // Each contour edge emits one wall quad and two bevel quads per segment;
    // cap triangulation adds at most a linear number of faces.
    const std::uint64_t estimatedFacesPerContourPoint =
        static_cast<std::uint64_t>(2 * effectiveBevelSegments + 8);
    // RenderData expands each wall/bevel quad to six triangle corners. Cap
    // corners stay linear in contour points, including merged-hole bridges.
    const std::uint64_t estimatedRenderVerticesPerContourPoint =
        static_cast<std::uint64_t>(12 * effectiveBevelSegments + 16);
    if (flattenedPointCount > maxEstimatedMeshVertices
                                  / estimatedVerticesPerContourPoint
        || flattenedPointCount > maxEstimatedMeshFaces
                                  / estimatedFacesPerContourPoint
        || flattenedPointCount > maxEstimatedRenderVertices
                                  / estimatedRenderVerticesPerContourPoint) {
        return fail(Procedural3DGenerationStatus::ResourceLimitExceeded,
                    QStringLiteral("Estimated extrusion or render mesh exceeds its resource budget."));
    }
    ShapeExtrudeParams extrudeParams;
    extrudeParams.depth = std::clamp(settings.depth, 0.0f, 100000.0f);
    extrudeParams.bevelWidth = std::clamp(settings.bevelWidth, 0.0f, 100000.0f);
    extrudeParams.bevelSegments = std::max(1, effectiveBevelSegments);
    Mesh mesh;
    if (!extrudeContourMesh(contours, extrudeParams, mesh)) {
        return fail(Procedural3DGenerationStatus::InvalidGeometry,
                    QStringLiteral("Text contours could not be triangulated into a closed extrusion."));
    }
    const Mesh::RenderData renderData = mesh.generateRenderData();
    if (renderData.positions.isEmpty() || renderData.indices.size() < 3u) {
        return fail(Procedural3DGenerationStatus::InvalidGeometry,
                    QStringLiteral("Extrusion produced an empty render mesh."));
    }
    if (renderData.indices.size() % 3u != 0u
        || renderData.normals.size() != renderData.positions.size()
        || renderData.uvs.size() != renderData.positions.size()) {
        return fail(Procedural3DGenerationStatus::InvalidGeometry,
                    QStringLiteral("Extrusion attributes or triangle indices are inconsistent."));
    }
    for (int index = 0; index < renderData.positions.size(); ++index) {
        const QVector3D& position = renderData.positions[index];
        const QVector3D& normal = renderData.normals[index];
        const QVector2D& uv = renderData.uvs[index];
        if (!std::isfinite(position.x()) || !std::isfinite(position.y())
            || !std::isfinite(position.z()) || !std::isfinite(normal.x())
            || !std::isfinite(normal.y()) || !std::isfinite(normal.z())
            || !std::isfinite(uv.x()) || !std::isfinite(uv.y())
            || normal.lengthSquared() < 1.0e-12f) {
            return fail(Procedural3DGenerationStatus::InvalidGeometry,
                        QStringLiteral("Extrusion contains non-finite attributes or a zero normal."));
        }
    }
    for (int index = 0; index < renderData.indices.size(); index += 3) {
        const unsigned int a = renderData.indices[index];
        const unsigned int b = renderData.indices[index + 1];
        const unsigned int c = renderData.indices[index + 2];
        if (a >= static_cast<unsigned int>(renderData.positions.size())
            || b >= static_cast<unsigned int>(renderData.positions.size())
            || c >= static_cast<unsigned int>(renderData.positions.size())) {
            return fail(Procedural3DGenerationStatus::InvalidGeometry,
                        QStringLiteral("Extrusion contains an out-of-range triangle index."));
        }
        const QVector3D area = QVector3D::crossProduct(
            renderData.positions[static_cast<int>(b)] - renderData.positions[static_cast<int>(a)],
            renderData.positions[static_cast<int>(c)] - renderData.positions[static_cast<int>(a)]);
        if (!std::isfinite(area.lengthSquared()) || area.lengthSquared() <= 1.0e-16f) {
            return fail(Procedural3DGenerationStatus::InvalidGeometry,
                        QStringLiteral("Extrusion contains a degenerate triangle."));
        }
    }
    const int vertexCount = renderData.positions.size();
    meshData.vertices.reserve(static_cast<std::size_t>(vertexCount));
    for (int index = 0; index < vertexCount; ++index) {
        Procedural3DVertex vertex;
        vertex.px = renderData.positions[index].x();
        vertex.py = renderData.positions[index].y();
        vertex.pz = renderData.positions[index].z();
        if (index < renderData.normals.size()) {
            vertex.nx = renderData.normals[index].x();
            vertex.ny = renderData.normals[index].y();
            vertex.nz = renderData.normals[index].z();
        }
        if (index < renderData.uvs.size()) {
            vertex.u = renderData.uvs[index].x();
            vertex.v = renderData.uvs[index].y();
        }
        meshData.vertices.push_back(vertex);
    }
    meshData.indices.reserve(renderData.indices.size());
    for (const unsigned int corner : renderData.indices) {
        meshData.indices.push_back(static_cast<std::uint32_t>(corner));
    }
    if (diagnostics != nullptr && diagnostics->missingGlyphCount > 0u) {
        diagnostics->status = Procedural3DGenerationStatus::MissingGlyphs;
        diagnostics->message = QStringLiteral(
            "%1 shaped glyph(s) resolved to the font's missing-glyph entry.")
            .arg(diagnostics->missingGlyphCount);
    }
    if (diagnostics != nullptr && !diagnostics->fontIdentityComplete) {
        if (!diagnostics->message.isEmpty()) diagnostics->message += QLatin1Char(' ');
        diagnostics->message += QStringLiteral(
            "One or more resolved font identities could not be fully recorded or fingerprinted.");
    }
    return meshData;
}

Procedural3DResult Procedural3DGenerators::generateTextExtrudeResult(const TextExtrudeSettings& settings,
                                                                    float timeSeconds,
                                                                    Procedural3DShading shading)
{
    Procedural3DResult result;
    result.kind = Procedural3DKind::TextExtrude;
    result.shading = shading;
    result.quality = settings.quality;
    TextExtrudeDiagnostics diagnostics;
    result.mesh = generateTextExtrude(settings, timeSeconds, &diagnostics);
    result.valid = !result.mesh.vertices.empty();
    result.status = diagnostics.status;
    result.diagnosticMessage = diagnostics.message;
    result.resolvedFontFamily = diagnostics.resolvedFontFamily;
    result.resolvedFontFamilies = diagnostics.resolvedFontFamilies;
    result.resolvedFontFingerprints = diagnostics.resolvedFontFingerprints;
    result.fontIdentityComplete = diagnostics.fontIdentityComplete;
    result.missingGlyphCount = diagnostics.missingGlyphCount;
    result.missingGlyphLocations = diagnostics.missingGlyphLocations;
    result.missingGlyphLocationsComplete = diagnostics.missingGlyphLocationsComplete;
    if (!result.valid && result.status == Procedural3DGenerationStatus::Success) {
        result.status = Procedural3DGenerationStatus::InvalidGeometry;
        result.diagnosticMessage = QStringLiteral("Text extrusion did not produce a mesh.");
    }
    return result;
}

TextExtrudeSettings Procedural3DGenerators::makeTextExtrudePreset(Procedural3DQuality quality)
{
    TextExtrudeSettings settings;
    settings.text = QStringLiteral("TEXT");
    settings.fontSize = 160;
    settings.depth = 40.0f;
    settings.bevelWidth = 4.0f;
    settings.bevelSegments = 2;
    settings.quality = quality;
    return settings;
}

} // namespace ArtifactCore
