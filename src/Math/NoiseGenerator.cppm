module;

#include <cmath>
#include <algorithm>
#include <vector>
#include <numeric>
#include <random>

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
    // 簡易Worley Noise
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
                
                // 各セル内のランダムな点
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

} // namespace ArtifactCore
