module;
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

module Render.NoiseField;

namespace ArtifactCore::RayTrace {

namespace {

inline std::uint32_t pcgHash(std::uint32_t input) noexcept {
    const std::uint32_t state = input * 747796405u + 2891336453u;
    const std::uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

inline std::uint32_t hashIJ(std::uint32_t i, std::uint32_t j, std::uint32_t seed) noexcept {
    return pcgHash(seed + i * 131071u + j * 641u);
}

inline std::uint32_t hashIJK(std::uint32_t i, std::uint32_t j, std::uint32_t k, std::uint32_t seed) noexcept {
    return pcgHash(seed + i * 131071u + j * 641u + k * 37u);
}

inline float randFloat01(std::uint32_t hash) noexcept {
    return static_cast<float>(hash & 0x7FFFFFFFu) / static_cast<float>(0x7FFFFFFFu);
}

inline float smoothStep(float t) noexcept {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

inline int fastFloor(float x) noexcept {
    const int i = static_cast<int>(x);
    return i - (x < static_cast<float>(i) ? 1 : 0);
}

inline float mod289(float x) noexcept {
    return x - std::floor(x * (1.0f / 289.0f)) * 289.0f;
}

inline float permute(float x) noexcept {
    return mod289((x * 34.0f + 10.0f) * x);
}

inline float taylorInvSqrt(float r) noexcept {
    return 1.79284291400159f - 0.85373472095314f * r;
}

inline void sincos_glm(float angle, float& s, float& c) noexcept {
    s = std::sin(angle);
    c = std::cos(angle);
}

struct Grad2 {
    float dx, dy;
};

struct Grad3 {
    float dx, dy, dz;
};

static const std::array<Grad2, 8> grad2Table = {{
    {-1.0f, -1.0f}, { 1.0f, -1.0f}, {-1.0f,  1.0f}, { 1.0f,  1.0f},
    {-1.0f,  0.0f}, { 1.0f,  0.0f}, { 0.0f, -1.0f}, { 0.0f,  1.0f},
}};

static const std::array<Grad3, 12> grad3Table = {{
    { 1.0f,  1.0f,  0.0f}, {-1.0f,  1.0f,  0.0f}, { 1.0f, -1.0f,  0.0f}, {-1.0f, -1.0f,  0.0f},
    { 1.0f,  0.0f,  1.0f}, {-1.0f,  0.0f,  1.0f}, { 1.0f,  0.0f, -1.0f}, {-1.0f,  0.0f, -1.0f},
    { 0.0f,  1.0f,  1.0f}, { 0.0f, -1.0f,  1.0f}, { 0.0f,  1.0f, -1.0f}, { 0.0f, -1.0f, -1.0f},
}};

inline Grad2 grad2(std::uint32_t hash) noexcept {
    return grad2Table[hash & 7u];
}

inline Grad3 grad3(std::uint32_t hash) noexcept {
    return grad3Table[hash % 12u];
}

float perlinNoise2D(float x, float y, std::uint32_t seed) noexcept {
    const int xi = fastFloor(x);
    const int yi = fastFloor(y);
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const float u = smoothStep(xf);
    const float v = smoothStep(yf);

    const auto g00 = grad2(hashIJ(xi, yi, seed));
    const auto g10 = grad2(hashIJ(xi + 1, yi, seed));
    const auto g01 = grad2(hashIJ(xi, yi + 1, seed));
    const auto g11 = grad2(hashIJ(xi + 1, yi + 1, seed));

    const float n00 = g00.dx * xf + g00.dy * yf;
    const float n10 = g10.dx * (xf - 1.0f) + g10.dy * yf;
    const float n01 = g01.dx * xf + g01.dy * (yf - 1.0f);
    const float n11 = g11.dx * (xf - 1.0f) + g11.dy * (yf - 1.0f);

    const float nx0 = lerp(n00, n10, u);
    const float nx1 = lerp(n01, n11, u);
    return lerp(nx0, nx1, v) * 0.5f + 0.5f;
}

float perlinNoise3D(float x, float y, float z, std::uint32_t seed) noexcept {
    const int xi = fastFloor(x);
    const int yi = fastFloor(y);
    const int zi = fastFloor(z);
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const float zf = z - static_cast<float>(zi);
    const float u = smoothStep(xf);
    const float v = smoothStep(yf);
    const float w = smoothStep(zf);

    const auto g000 = grad3(hashIJK(xi, yi, zi, seed));
    const auto g100 = grad3(hashIJK(xi + 1, yi, zi, seed));
    const auto g010 = grad3(hashIJK(xi, yi + 1, zi, seed));
    const auto g110 = grad3(hashIJK(xi + 1, yi + 1, zi, seed));
    const auto g001 = grad3(hashIJK(xi, yi, zi + 1, seed));
    const auto g101 = grad3(hashIJK(xi + 1, yi, zi + 1, seed));
    const auto g011 = grad3(hashIJK(xi, yi + 1, zi + 1, seed));
    const auto g111 = grad3(hashIJK(xi + 1, yi + 1, zi + 1, seed));

    const float n000 = g000.dx * xf + g000.dy * yf + g000.dz * zf;
    const float n100 = g100.dx * (xf - 1.0f) + g100.dy * yf + g100.dz * zf;
    const float n010 = g010.dx * xf + g010.dy * (yf - 1.0f) + g010.dz * zf;
    const float n110 = g110.dx * (xf - 1.0f) + g110.dy * (yf - 1.0f) + g110.dz * zf;
    const float n001 = g001.dx * xf + g001.dy * yf + g001.dz * (zf - 1.0f);
    const float n101 = g101.dx * (xf - 1.0f) + g101.dy * yf + g101.dz * (zf - 1.0f);
    const float n011 = g011.dx * xf + g011.dy * (yf - 1.0f) + g011.dz * (zf - 1.0f);
    const float n111 = g111.dx * (xf - 1.0f) + g111.dy * (yf - 1.0f) + g111.dz * (zf - 1.0f);

    const float nx00 = lerp(n000, n100, u);
    const float nx10 = lerp(n010, n110, u);
    const float nx01 = lerp(n001, n101, u);
    const float nx11 = lerp(n011, n111, u);
    const float nxy0 = lerp(nx00, nx10, v);
    const float nxy1 = lerp(nx01, nx11, v);
    return lerp(nxy0, nxy1, w) * 0.5f + 0.5f;
}

float simplexNoise2D(float x, float y, std::uint32_t seed) noexcept {
    const float F2 = 0.5f * (std::sqrt(3.0f) - 1.0f);
    const float G2 = (3.0f - std::sqrt(3.0f)) / 6.0f;

    const float s = (x + y) * F2;
    const int i = fastFloor(x + s);
    const int j = fastFloor(y + s);
    const float t = static_cast<float>(i + j) * G2;
    const float X0 = static_cast<float>(i) - t;
    const float Y0 = static_cast<float>(j) - t;
    const float x0 = x - X0;
    const float y0 = y - Y0;

    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else { i1 = 0; j1 = 1; }

    const float x1 = x0 - static_cast<float>(i1) + G2;
    const float y1 = y0 - static_cast<float>(j1) + G2;
    const float x2 = x0 - 1.0f + 2.0f * G2;
    const float y2 = y0 - 1.0f + 2.0f * G2;

    const std::uint32_t gi0 = hashIJ(i, j, seed) % 12u;
    const std::uint32_t gi1 = hashIJ(i + i1, j + j1, seed) % 12u;
    const std::uint32_t gi2 = hashIJ(i + 1, j + 1, seed) % 12u;

    float n0 = 0.0f;
    float t0_ = 0.5f - x0 * x0 - y0 * y0;
    if (t0_ >= 0.0f) {
        t0_ *= t0_;
        n0 = t0_ * t0_ * (grad3Table[gi0].dx * x0 + grad3Table[gi0].dy * y0);
    }

    float n1 = 0.0f;
    float t1_ = 0.5f - x1 * x1 - y1 * y1;
    if (t1_ >= 0.0f) {
        t1_ *= t1_;
        n1 = t1_ * t1_ * (grad3Table[gi1].dx * x1 + grad3Table[gi1].dy * y1);
    }

    float n2 = 0.0f;
    float t2_ = 0.5f - x2 * x2 - y2 * y2;
    if (t2_ >= 0.0f) {
        t2_ *= t2_;
        n2 = t2_ * t2_ * (grad3Table[gi2].dx * x2 + grad3Table[gi2].dy * y2);
    }

    return 35.0f * (n0 + n1 + n2) + 0.5f;
}

float simplexNoise3D(float x, float y, float z, std::uint32_t seed) noexcept {
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;

    const float s = (x + y + z) * F3;
    const int i = fastFloor(x + s);
    const int j = fastFloor(y + s);
    const int k = fastFloor(z + s);
    const float t = static_cast<float>(i + j + k) * G3;
    const float X0 = static_cast<float>(i) - t;
    const float Y0 = static_cast<float>(j) - t;
    const float Z0 = static_cast<float>(k) - t;
    const float x0 = x - X0;
    const float y0 = y - Y0;
    const float z0 = z - Z0;

    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0)        { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0)   { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else                 { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0)         { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0)    { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else                 { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    const float x1 = x0 - static_cast<float>(i1) + G3;
    const float y1 = y0 - static_cast<float>(j1) + G3;
    const float z1 = z0 - static_cast<float>(k1) + G3;
    const float x2 = x0 - static_cast<float>(i2) + 2.0f * G3;
    const float y2 = y0 - static_cast<float>(j2) + 2.0f * G3;
    const float z2 = z0 - static_cast<float>(k2) + 2.0f * G3;
    const float x3 = x0 - 1.0f + 3.0f * G3;
    const float y3 = y0 - 1.0f + 3.0f * G3;
    const float z3 = z0 - 1.0f + 3.0f * G3;

    const std::uint32_t gi0 = hashIJK(i, j, k, seed) % 12u;
    const std::uint32_t gi1 = hashIJK(i + i1, j + j1, k + k1, seed) % 12u;
    const std::uint32_t gi2 = hashIJK(i + i2, j + j2, k + k2, seed) % 12u;
    const std::uint32_t gi3 = hashIJK(i + 1, j + 1, k + 1, seed) % 12u;

    auto contrib = [](float x, float y, float z, const Grad3& g) noexcept -> float {
        float t0 = 0.6f - x * x - y * y - z * z;
        if (t0 < 0.0f) return 0.0f;
        t0 *= t0;
        return t0 * t0 * (g.dx * x + g.dy * y + g.dz * z);
    };

    float n = contrib(x0, y0, z0, grad3Table[gi0])
            + contrib(x1, y1, z1, grad3Table[gi1])
            + contrib(x2, y2, z2, grad3Table[gi2])
            + contrib(x3, y3, z3, grad3Table[gi3]);

    return 32.0f * n + 0.5f;
}

float worleyNoise2D(float x, float y, std::uint32_t seed, bool inverse) noexcept {
    const int cx = fastFloor(x);
    const int cy = fastFloor(y);
    float minDistSq = std::numeric_limits<float>::max();

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const std::uint32_t h = hashIJ(cx + dx, cy + dy, seed);
            const float px = static_cast<float>(cx + dx) + randFloat01(h);
            const float py = static_cast<float>(cy + dy) + randFloat01(h ^ 0x55555555u);
            const float ddx = x - px;
            const float ddy = y - py;
            const float dSq = ddx * ddx + ddy * ddy;
            minDistSq = std::min(minDistSq, dSq);
        }
    }

    float result = std::sqrt(minDistSq);
    return inverse ? 1.0f - std::min(result, 1.0f) : std::min(result, 1.0f);
}

float worleyNoise3D(float x, float y, float z, std::uint32_t seed, bool inverse) noexcept {
    const int cx = fastFloor(x);
    const int cy = fastFloor(y);
    const int cz = fastFloor(z);
    float minDistSq = std::numeric_limits<float>::max();

    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const std::uint32_t h = hashIJK(cx + dx, cy + dy, cz + dz, seed);
                const float px = static_cast<float>(cx + dx) + randFloat01(h);
                const float py = static_cast<float>(cy + dy) + randFloat01(h ^ 0x55555555u);
                const float pz = static_cast<float>(cz + dz) + randFloat01(h ^ 0xAAAAAAAAu);
                const float ddx = x - px;
                const float ddy = y - py;
                const float ddz = z - pz;
                const float dSq = ddx * ddx + ddy * ddy + ddz * ddz;
                minDistSq = std::min(minDistSq, dSq);
            }
        }
    }

    float result = std::sqrt(minDistSq);
    return inverse ? 1.0f - std::min(result, 1.0f) : std::min(result, 1.0f);
}

} // anonymous namespace

float fbm2D(float x, float y, const NoiseSettings& settings) noexcept {
    float value = 0.0f;
    float amp = settings.amplitude;
    float freq = settings.frequency;
    float totalAmp = 0.0f;

    for (int i = 0; i < settings.octaves; ++i) {
        const float px = (x + settings.offset.x) * freq;
        const float py = (y + settings.offset.y) * freq;
        value += perlinNoise2D(px, py, settings.seed + static_cast<std::uint32_t>(i) * 7919u) * amp;
        totalAmp += amp;
        amp *= settings.gain;
        freq *= settings.lacunarity;
    }

    return value / (totalAmp + 1e-10f);
}

float fbm3D(float x, float y, float z, const NoiseSettings& settings) noexcept {
    float value = 0.0f;
    float amp = settings.amplitude;
    float freq = settings.frequency;
    float totalAmp = 0.0f;

    for (int i = 0; i < settings.octaves; ++i) {
        const float px = (x + settings.offset.x) * freq;
        const float py = (y + settings.offset.y) * freq;
        const float pz = (z + settings.offset.z) * freq;
        value += perlinNoise3D(px, py, pz, settings.seed + static_cast<std::uint32_t>(i) * 7919u) * amp;
        totalAmp += amp;
        amp *= settings.gain;
        freq *= settings.lacunarity;
    }

    return value / (totalAmp + 1e-10f);
}

float turbulence2D(float x, float y, const NoiseSettings& settings) noexcept {
    float value = 0.0f;
    float amp = settings.amplitude;
    float freq = settings.frequency;
    float totalAmp = 0.0f;

    for (int i = 0; i < settings.octaves; ++i) {
        const float px = (x + settings.offset.x) * freq;
        const float py = (y + settings.offset.y) * freq;
        const float n = perlinNoise2D(px, py, settings.seed + static_cast<std::uint32_t>(i) * 7919u);
        value += std::abs(n * 2.0f - 1.0f) * amp;
        totalAmp += amp;
        amp *= settings.gain;
        freq *= settings.lacunarity;
    }

    return value / (totalAmp + 1e-10f);
}

float turbulence3D(float x, float y, float z, const NoiseSettings& settings) noexcept {
    float value = 0.0f;
    float amp = settings.amplitude;
    float freq = settings.frequency;
    float totalAmp = 0.0f;

    for (int i = 0; i < settings.octaves; ++i) {
        const float px = (x + settings.offset.x) * freq;
        const float py = (y + settings.offset.y) * freq;
        const float pz = (z + settings.offset.z) * freq;
        const float n = perlinNoise3D(px, py, pz, settings.seed + static_cast<std::uint32_t>(i) * 7919u);
        value += std::abs(n * 2.0f - 1.0f) * amp;
        totalAmp += amp;
        amp *= settings.gain;
        freq *= settings.lacunarity;
    }

    return value / (totalAmp + 1e-10f);
}

float domainWarp2D(float x, float y, const NoiseSettings& settings) noexcept {
    const float warpX = perlinNoise2D(x, y, settings.seed) * 2.0f - 1.0f;
    const float warpY = perlinNoise2D(x + 5.2f, y + 1.3f, settings.seed) * 2.0f - 1.0f;

    return fbm2D(
        x + warpX * settings.domainWarpStrength,
        y + warpY * settings.domainWarpStrength,
        settings
    );
}

float domainWarp3D(float x, float y, float z, const NoiseSettings& settings) noexcept {
    const float warpX = perlinNoise3D(x, y, z, settings.seed) * 2.0f - 1.0f;
    const float warpY = perlinNoise3D(x + 5.2f, y + 1.3f, z + 9.7f, settings.seed) * 2.0f - 1.0f;
    const float warpZ = perlinNoise3D(x - 3.8f, y + 7.1f, z - 2.4f, settings.seed) * 2.0f - 1.0f;

    return fbm3D(
        x + warpX * settings.domainWarpStrength,
        y + warpY * settings.domainWarpStrength,
        z + warpZ * settings.domainWarpStrength,
        settings
    );
}

Vec3 curlNoise2D(float x, float y, const NoiseSettings& settings) noexcept {
    const float eps = 0.001f;
    const float nx = fbm2D(x + eps, y, settings) - fbm2D(x - eps, y, settings);
    const float ny = fbm2D(x, y + eps, settings) - fbm2D(x, y - eps, settings);
    return {ny * 0.5f / eps, -nx * 0.5f / eps, 0.0f};
}

Vec3 curlNoise3D(float x, float y, float z, const NoiseSettings& settings) noexcept {
    const float eps = 0.001f;
    const float nx = fbm3D(x + eps, y, z, settings) - fbm3D(x - eps, y, z, settings);
    const float ny = fbm3D(x, y + eps, z, settings) - fbm3D(x, y - eps, z, settings);
    const float nz = fbm3D(x, y, z + eps, settings) - fbm3D(x, y, z - eps, settings);

    Vec3 grad1{fbm3D(x + eps, y, z, settings) - fbm3D(x - eps, y, z, settings),
               fbm3D(x, y + eps, z, settings) - fbm3D(x, y - eps, z, settings),
               fbm3D(x, y, z + eps, settings) - fbm3D(x, y, z - eps, settings)};

    Vec3 grad2{fbm3D(x + eps + 5.2f, y, z, settings) - fbm3D(x - eps + 5.2f, y, z, settings),
               fbm3D(x, y + eps + 5.2f, z, settings) - fbm3D(x, y - eps + 5.2f, z, settings),
               fbm3D(x, y, z + eps + 5.2f, settings) - fbm3D(x, y, z - eps + 5.2f, settings)};

    return {
        (grad1.y * grad2.z - grad1.z * grad2.y) * 0.5f / eps,
        (grad1.z * grad2.x - grad1.x * grad2.z) * 0.5f / eps,
        (grad1.x * grad2.y - grad1.y * grad2.x) * 0.5f / eps,
    };
}

void fillScalarField(VolumeScalarField& field, const NoiseScalarFunc& noiseFn) noexcept {
    if (field.empty() || !noiseFn) return;
    const auto res = field.resolution;
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    for (int z = 0; z < res.depth; ++z) {
        for (int y = 0; y < res.height; ++y) {
            for (int x = 0; x < res.width; ++x) {
                const float fx = static_cast<float>(x) * invW;
                const float fy = static_cast<float>(y) * invH;
                const float fz = static_cast<float>(z) * invD;
                field.at(x, y, z) = noiseFn(fx, fy, fz);
            }
        }
    }
}

void fillScalarFieldFBM(VolumeScalarField& field, const NoiseSettings& settings) noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    for (int z = 0; z < res.depth; ++z) {
        for (int y = 0; y < res.height; ++y) {
            for (int x = 0; x < res.width; ++x) {
                const float fx = static_cast<float>(x) * invW;
                const float fy = static_cast<float>(y) * invH;
                const float fz = static_cast<float>(z) * invD;
                field.at(x, y, z) = fbm3D(fx, fy, fz, settings);
            }
        }
    }
}

void fillScalarFieldTurbulence(VolumeScalarField& field, const NoiseSettings& settings) noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    for (int z = 0; z < res.depth; ++z) {
        for (int y = 0; y < res.height; ++y) {
            for (int x = 0; x < res.width; ++x) {
                const float fx = static_cast<float>(x) * invW;
                const float fy = static_cast<float>(y) * invH;
                const float fz = static_cast<float>(z) * invD;
                field.at(x, y, z) = turbulence3D(fx, fy, fz, settings);
            }
        }
    }
}

void fillScalarFieldWorley(VolumeScalarField& field, bool inverse, std::uint32_t seed) noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    for (int z = 0; z < res.depth; ++z) {
        for (int y = 0; y < res.height; ++y) {
            for (int x = 0; x < res.width; ++x) {
                const float fx = static_cast<float>(x) * invW;
                const float fy = static_cast<float>(y) * invH;
                const float fz = static_cast<float>(z) * invD;
                field.at(x, y, z) = worleyNoise3D(fx, fy, fz, seed, inverse);
            }
        }
    }
}

void fillVectorFieldCurl(VolumeVectorField& field, const NoiseSettings& settings) noexcept {
    if (field.empty()) return;
    const auto res = field.resolution;
    const float invW = res.width > 1 ? 1.0f / static_cast<float>(res.width) : 1.0f;
    const float invH = res.height > 1 ? 1.0f / static_cast<float>(res.height) : 1.0f;
    const float invD = res.depth > 1 ? 1.0f / static_cast<float>(res.depth) : 1.0f;

    for (int z = 0; z < res.depth; ++z) {
        for (int y = 0; y < res.height; ++y) {
            for (int x = 0; x < res.width; ++x) {
                const float fx = static_cast<float>(x) * invW;
                const float fy = static_cast<float>(y) * invH;
                const float fz = static_cast<float>(z) * invD;
                field.at(x, y, z) = curlNoise3D(fx, fy, fz, settings);
            }
        }
    }
}

} // namespace ArtifactCore::RayTrace