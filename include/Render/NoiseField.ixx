module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

export module Render.NoiseField;

import Render.Vector3D;
import Render.VolumeRenderer;

export namespace ArtifactCore::RayTrace {

enum class NoiseKind : std::uint8_t {
    Perlin = 0,
    Simplex = 1,
    Worley = 2,
    WorleyInverse = 3,
    Curl = 4,
    Turbulence = 5,
    DomainWarp = 6,
};

struct NoiseSettings {
    std::uint32_t seed = 0;
    float frequency = 1.0f;
    int octaves = 4;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float amplitude = 1.0f;
    Vec3 offset{0.0f, 0.0f, 0.0f};
    float turbulencePower = 2.0f;
    float domainWarpStrength = 0.5f;
};

[[nodiscard]] float perlinNoise2D(float x, float y, std::uint32_t seed = 0) noexcept;
[[nodiscard]] float perlinNoise3D(float x, float y, float z, std::uint32_t seed = 0) noexcept;
[[nodiscard]] float simplexNoise2D(float x, float y, std::uint32_t seed = 0) noexcept;
[[nodiscard]] float simplexNoise3D(float x, float y, float z, std::uint32_t seed = 0) noexcept;
[[nodiscard]] float worleyNoise2D(float x, float y, std::uint32_t seed = 0, bool inverse = false) noexcept;
[[nodiscard]] float worleyNoise3D(float x, float y, float z, std::uint32_t seed = 0, bool inverse = false) noexcept;

[[nodiscard]] float fbm2D(float x, float y, const NoiseSettings& settings) noexcept;
[[nodiscard]] float fbm3D(float x, float y, float z, const NoiseSettings& settings) noexcept;
[[nodiscard]] float turbulence2D(float x, float y, const NoiseSettings& settings) noexcept;
[[nodiscard]] float turbulence3D(float x, float y, float z, const NoiseSettings& settings) noexcept;

[[nodiscard]] float domainWarp2D(float x, float y, const NoiseSettings& settings) noexcept;
[[nodiscard]] float domainWarp3D(float x, float y, float z, const NoiseSettings& settings) noexcept;

[[nodiscard]] Vec3 curlNoise2D(float x, float y, const NoiseSettings& settings) noexcept;
[[nodiscard]] Vec3 curlNoise3D(float x, float y, float z, const NoiseSettings& settings) noexcept;

using NoiseScalarFunc = std::function<float(float x, float y, float z)>;
using NoiseVectorFunc = std::function<Vec3(float x, float y, float z)>;

void fillScalarField(VolumeScalarField& field, const NoiseScalarFunc& noiseFn) noexcept;
void fillScalarFieldFBM(VolumeScalarField& field, const NoiseSettings& settings) noexcept;
void fillScalarFieldTurbulence(VolumeScalarField& field, const NoiseSettings& settings) noexcept;
void fillScalarFieldWorley(VolumeScalarField& field, bool inverse, std::uint32_t seed = 0) noexcept;
void fillVectorFieldCurl(VolumeVectorField& field, const NoiseSettings& settings) noexcept;

} // namespace ArtifactCore::RayTrace