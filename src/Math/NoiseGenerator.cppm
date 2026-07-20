module;

#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <numeric>
#include <random>
#include <QString>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Math.Noise;

namespace ArtifactCore {

// 順列テーブルの初期化
static int p[512];
static bool isInitialized = false;

void NoiseGenerator::setSeed(unsigned int seed) {
    std::vector<int> permutation(256);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::default_random_engine engine(seed);
    std::shuffle(permutation.begin(), permutation.end(), engine);

    for (int i = 0; i < 256; i++) {
        p[i] = p[i + 256] = permutation[i];
    }
    isInitialized = true;
}

static void ensureInitialized() {
    if (!isInitialized) {
        NoiseGenerator::setSeed(42); // Default seed
    }
}

float NoiseGenerator::fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float NoiseGenerator::lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float NoiseGenerator::grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float NoiseGenerator::perlin(float x, float y, float z) {
    ensureInitialized();

    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
    int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

    return lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
                                    grad(p[BA], x - 1, y, z)),
                           lerp(u, grad(p[AB], x, y - 1, z),
                                    grad(p[BB], x - 1, y - 1, z))),
                   lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1),
                                    grad(p[BA + 1], x - 1, y, z - 1)),
                           lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                                    grad(p[BB + 1], x - 1, y - 1, z - 1))));
}

float NoiseGenerator::perlin(float x) {
    return perlin(x, 0.0f, 0.0f);
}

float NoiseGenerator::perlin(float x, float y) {
    return perlin(x, y, 0.0f);
}

float NoiseGenerator::fractal(float x, float y, float z, int octaves, float persistence, float lacunarity) {
    float total = 0;
    float frequency = 1;
    float amplitude = 1;
    float maxValue = 0; // 最大振幅の合計で正規化するため
    for (int i = 0; i < octaves; i++) {
        total += perlin(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxValue;
}

float NoiseGenerator::worley(float x, float y, float z) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    int zi = static_cast<int>(std::floor(z));

    float minDist = 1.0e10;

    auto hash = [](int x, int y, int z) -> float {
        unsigned int h = x * 73856093 ^ y * 19349663 ^ z * 83492791;
        return (h % 1000) / 1000.0f;
    };

    for (int dz = -1; dz <= 1; dz++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int cx = xi + dx;
                int cy = yi + dy;
                int cz = zi + dz;
                
                float px = cx + hash(cx, cy, cz);
                float py = cy + hash(cy, cz, cx);
                float pz = cz + hash(cz, cx, cy);

                float dist = std::sqrt(std::pow(x - px, 2) + std::pow(y - py, 2) + std::pow(z - pz, 2));
                minDist = std::min(minDist, dist);
            }
        }
    }
    return minDist;
}

NoiseField::NoiseField(unsigned int seed)
{
    setSeed(seed);
}

void NoiseField::setSeed(unsigned int seed)
{
    seed_ = seed;
    std::array<int, 256> permutation{};
    std::iota(permutation.begin(), permutation.end(), 0);
    std::default_random_engine engine(seed);
    std::shuffle(permutation.begin(), permutation.end(), engine);
    for (int i = 0; i < 256; ++i) {
        permutation_[static_cast<std::size_t>(i)] = permutation[static_cast<std::size_t>(i)];
        permutation_[static_cast<std::size_t>(i + 256)] = permutation[static_cast<std::size_t>(i)];
    }
}

float NoiseField::fade(float t) noexcept
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float NoiseField::lerp(float t, float a, float b) noexcept
{
    return a + t * (b - a);
}

float NoiseField::grad(int hash, float x, float y, float z) noexcept
{
    const int h = hash & 15;
    const float u = h < 8 ? x : y;
    const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float NoiseField::perlin(float x, float y, float z) const noexcept
{
    const int X = static_cast<int>(std::floor(x)) & 255;
    const int Y = static_cast<int>(std::floor(y)) & 255;
    const int Z = static_cast<int>(std::floor(z)) & 255;
    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);
    const float u = fade(x);
    const float v = fade(y);
    const float w = fade(z);
    const int A = permutation_[X] + Y;
    const int AA = permutation_[A] + Z;
    const int AB = permutation_[A + 1] + Z;
    const int B = permutation_[X + 1] + Y;
    const int BA = permutation_[B] + Z;
    const int BB = permutation_[B + 1] + Z;
    return lerp(w,
        lerp(v, lerp(u, grad(permutation_[AA], x, y, z),
                         grad(permutation_[BA], x - 1.0f, y, z)),
                  lerp(u, grad(permutation_[AB], x, y - 1.0f, z),
                         grad(permutation_[BB], x - 1.0f, y - 1.0f, z))),
        lerp(v, lerp(u, grad(permutation_[AA + 1], x, y, z - 1.0f),
                         grad(permutation_[BA + 1], x - 1.0f, y, z - 1.0f)),
                  lerp(u, grad(permutation_[AB + 1], x, y - 1.0f, z - 1.0f),
                         grad(permutation_[BB + 1], x - 1.0f, y - 1.0f, z - 1.0f))));
}

float NoiseField::fractal(float x, float y, float z, int octaves,
                          float persistence, float lacunarity) const noexcept
{
    if (octaves <= 0) return 0.0f;
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float normalizer = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        total += perlin(x * frequency, y * frequency, z * frequency) * amplitude;
        normalizer += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return normalizer > 0.0f ? total / normalizer : 0.0f;
}

// ============================================================
// GPU Shader Source Generation (for wiggle())
// ============================================================

QString NoiseGenerator::glslPerlin2D() const {
    return QStringLiteral(R"(
vec2 perlinPos = pos * frequency;
vec2 i = floor(perlinPos);
vec2 f = fract(perlinPos);
vec2 u = f * f * (3.0 - 2.0 * f);
float a = texture(noisePermTex, (i + vec2(0.0, 0.0)) / 256.0).x;
float b = texture(noisePermTex, (i + vec2(1.0, 0.0)) / 256.0).x;
float c = texture(noisePermTex, (i + vec2(0.0, 1.0)) / 256.0).x;
float d = texture(noisePermTex, (i + vec2(1.0, 1.0)) / 256.0).x;
float value = mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
)");
}

QString NoiseGenerator::glslWorley2D() const {
    return QStringLiteral(R"(
vec2 cell = floor(pos * frequency);
vec2 fracPos = fract(pos * frequency);
float minDist = 1e10;
for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
        vec2 neighbor = cell + vec2(float(dx), float(dy));
        float seed = float(dx * 73856093 ^ dy * 19349663);
        vec2 point = neighbor + vec2(fract(sin(seed) * 43758.5453), fract(cos(seed) * 43758.5453));
        float dist = length(fracPos - point);
        minDist = min(minDist, dist);
    }
}
)");
}

QString NoiseGenerator::wiggleGLSL(float freq, float amp, int octaves, 
                                    float persistence, float lacunarity) const {
    return QStringLiteral(R"(
float noiseVal = perlinValue(pos * %1);
float ampFactor = %2;
for (int i = 1; i < %3; i++) {
    noiseVal += texture(noiseTex, pos * %1 * pow(%4, float(i))) * ampFactor;
    ampFactor *= %5;
}
result = base + noiseVal * %2;
)").arg(QString::number(freq), QString::number(amp), 
       QString::number(octaves), QString::number(lacunarity), 
       QString::number(persistence));
}

} // namespace ArtifactCore
