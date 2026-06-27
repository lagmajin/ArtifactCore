module;

#include <array>
#include <cstdint>
#include <vector>

export module Procedural3DGenerators;

import Math.Noise;

export namespace ArtifactCore {

enum class Procedural3DKind : std::uint32_t {
    Terrain = 0,
    PathTube = 1
};

enum class Procedural3DQuality : std::uint32_t {
    Draft = 0,
    Preview = 1,
    Full = 2
};

enum class Procedural3DShading : std::uint32_t {
    Wire = 0,
    Solid = 1,
    Normal = 2,
    Lit = 3
};

enum class ProceduralPathProfile : std::uint32_t {
    Tube = 0,
    Ribbon = 1
};

enum class ProceduralPathSource : std::uint32_t {
    Parametric = 0,
    ControlPoints = 1
};

enum class TerrainHeightSource : std::uint32_t {
    Noise = 0,
    ImageLuminance = 1,
    AudioAmplitude = 2
};

enum class TerrainUvMode : std::uint32_t {
    Grid = 0,
    Planar = 1,
    Polar = 2
};

struct Procedural3DVertex {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 1.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct Procedural3DMeshData {
    std::vector<Procedural3DVertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct TerrainSettings {
    std::uint32_t seed = 1u;
    TerrainHeightSource heightSource = TerrainHeightSource::Noise;
    TerrainUvMode uvMode = TerrainUvMode::Grid;
    int columns = 64;
    int rows = 64;
    float sizeX = 10.0f;
    float sizeY = 10.0f;
    float height = 2.0f;
    float noiseScale = 1.0f;
    float noiseAmplitude = 1.0f;
    int noiseOctaves = 4;
    float noiseEvolution = 0.0f;
    int heightSampleWidth = 0;
    int heightSampleHeight = 0;
    std::vector<float> heightSamples;
    float audioAmplitude = 0.0f;
    bool audioAvailable = false;
    Procedural3DQuality quality = Procedural3DQuality::Preview;
};

struct PathTubeSettings {
    std::uint32_t seed = 1u;
    ProceduralPathSource pathSource = ProceduralPathSource::Parametric;
    ProceduralPathProfile profile = ProceduralPathProfile::Tube;
    bool pathClosed = false;
    std::vector<std::array<float, 3>> pathPoints;
    int pathSamples = 128;
    int sides = 8;
    float radius = 0.15f;
    float taperStart = 1.0f;
    float taperEnd = 1.0f;
    float twist = 0.0f;
    float pathOffset = 0.0f;
    float repeatCount = 1.0f;
    float pathScale = 1.0f;
    float noiseScale = 1.0f;
    float noiseAmplitude = 0.0f;
    Procedural3DQuality quality = Procedural3DQuality::Preview;
};

struct Procedural3DResult {
    Procedural3DKind kind = Procedural3DKind::Terrain;
    Procedural3DShading shading = Procedural3DShading::Solid;
    Procedural3DQuality quality = Procedural3DQuality::Preview;
    Procedural3DMeshData mesh;
    bool valid = false;
};

struct Procedural3DBounds {
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
};

struct Procedural3DPreviewInfo {
    Procedural3DKind kind = Procedural3DKind::Terrain;
    Procedural3DShading shading = Procedural3DShading::Solid;
    Procedural3DQuality quality = Procedural3DQuality::Preview;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    Procedural3DBounds bounds {};
};

class Procedural3DGenerators {
public:
    static Procedural3DMeshData generateTerrain(const TerrainSettings& settings, float timeSeconds = 0.0f);
    static Procedural3DMeshData generatePathTube(const PathTubeSettings& settings, float timeSeconds = 0.0f);

    static Procedural3DResult generateTerrainResult(const TerrainSettings& settings,
                                                    float timeSeconds = 0.0f,
                                                    Procedural3DShading shading = Procedural3DShading::Solid);
    static Procedural3DResult generatePathTubeResult(const PathTubeSettings& settings,
                                                    float timeSeconds = 0.0f,
                                                    Procedural3DShading shading = Procedural3DShading::Solid);

    static TerrainSettings makeTerrainPreset(std::uint32_t seed = 1u, Procedural3DQuality quality = Procedural3DQuality::Preview);
    static PathTubeSettings makePathTubePreset(std::uint32_t seed = 1u, Procedural3DQuality quality = Procedural3DQuality::Preview);
    static Procedural3DPreviewInfo previewInfo(const Procedural3DResult& result);

private:
    static int clampGridCount(int value, int minimum, int maximum);
    static float clamp01(float value);
    static float safeSqrt(float value);
    static Procedural3DBounds computeBounds(const Procedural3DMeshData& mesh);
    static void computeTerrainNormal(Procedural3DMeshData& mesh, int columns, int rows);
};

} // namespace ArtifactCore
