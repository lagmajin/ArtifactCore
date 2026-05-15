module;
#include <utility>
#include <random>
#include <cstdint>
#include <cmath>

export module Math.Random;

export namespace ArtifactCore {

struct float2 { float x, y; };
struct float3 { float x, y, z; };

// ============================================================
// Unified Random Utilities
// ============================================================

class Random {
public:
    static Random& instance() {
        static Random rng;
        return rng;
    }

    // Seed
    void seed(uint64_t s) { engine_.seed(s); }

    // Float in [0, 1)
    float float01() {
        return dist01_(engine_);
    }

    // Float in [min, max)
    float floatRange(float min, float max) {
        return min + (max - min) * dist01_(engine_);
    }

    // Integer in [min, max]
    int intRange(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine_);
    }

    // Gaussian (mean=0, stddev=1)
    float gaussian() {
        return distGauss_(engine_);
    }

    // Gaussian with custom mean/stddev
    float gaussian(float mean, float stddev) {
        return mean + stddev * distGauss_(engine_);
    }

    // Bool with probability p (0.0 ~ 1.0)
    bool coin(float p = 0.5f) {
        return dist01_(engine_) < p;
    }

    // Random 2D direction (unit vector)
    float2 direction2D() {
        float angle = floatRange(0.0f, 6.28318530718f);
        return { std::cos(angle), std::sin(angle) };
    }

    // Random 3D direction (unit vector on sphere)
    float3 direction3D() {
        float z = floatRange(-1.0f, 1.0f);
        float r = std::sqrt(1.0f - z * z);
        float angle = floatRange(0.0f, 6.28318530718f);
        return { r * std::cos(angle), r * std::sin(angle), z };
    }

    // Random color (RGBA, 0-1)
    struct Color4 { float r, g, b, a; };
    Color4 color(bool randomAlpha = false) {
        return {
            float01(),
            float01(),
            float01(),
            randomAlpha ? float01() : 1.0f
        };
    }

private:
    Random() : engine_(std::random_device{}()) {}
    std::mt19937 engine_;
    std::uniform_real_distribution<float> dist01_{0.0f, 1.0f};
    std::normal_distribution<float> distGauss_{0.0f, 1.0f};
};

// Standalone functions (for convenience)
export inline float randomFloat01() { return Random::instance().float01(); }
export inline float randomFloatRange(float min, float max) { return Random::instance().floatRange(min, max); }
export inline int randomIntRange(int min, int max) { return Random::instance().intRange(min, max); }
export inline float randomGaussian() { return Random::instance().gaussian(); }

}
