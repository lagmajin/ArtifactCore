module;
#include <utility>
#include <algorithm>
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <random>

export module Math.Random;

export namespace ArtifactCore {

struct float2 { float x, y; };
struct float3 { float x, y, z; };

class RandomStream {
public:
    explicit RandomStream(uint64_t seed = defaultSeed()) noexcept
    {
        this->seed(seed);
    }

    void seed(uint64_t seedValue) noexcept
    {
        state_ = seedValue == 0 ? defaultSeed() : seedValue;
        state_ ^= kStreamMix;
        state_ = splitmix64(state_);
    }

    uint64_t state() const noexcept
    {
        return state_;
    }

    uint64_t nextU64() noexcept
    {
        state_ += kGamma;
        return splitmix64(state_);
    }

    uint32_t nextU32() noexcept
    {
        return static_cast<uint32_t>(nextU64() >> 32);
    }

    float unitFloat() noexcept
    {
        return static_cast<float>((nextU64() >> 40) * kUnitFloat);
    }

    double unitDouble() noexcept
    {
        return static_cast<double>(nextU64() >> 11) * kUnitDouble;
    }

    float range(float minValue, float maxValue) noexcept
    {
        if (minValue > maxValue) {
            std::swap(minValue, maxValue);
        }
        return minValue + (maxValue - minValue) * unitFloat();
    }

    int rangeInclusive(int minValue, int maxValue) noexcept
    {
        if (minValue > maxValue) {
            std::swap(minValue, maxValue);
        }

        const uint64_t span = static_cast<uint64_t>(static_cast<int64_t>(maxValue) - static_cast<int64_t>(minValue) + 1);
        if (span == 0) {
            return minValue;
        }

        const uint64_t limit = std::numeric_limits<uint64_t>::max() -
                                (std::numeric_limits<uint64_t>::max() % span);
        uint64_t value = nextU64();
        while (value >= limit) {
            value = nextU64();
        }
        return minValue + static_cast<int>(value % span);
    }

    bool chance(float probability) noexcept
    {
        if (probability <= 0.0f) {
            return false;
        }
        if (probability >= 1.0f) {
            return true;
        }
        return unitFloat() < probability;
    }

    float gaussian(float mean = 0.0f, float stddev = 1.0f) noexcept
    {
        const float u1 = std::max(unitFloat(), std::numeric_limits<float>::min());
        const float u2 = unitFloat();
        const float radius = std::sqrt(-2.0f * std::log(u1));
        const float angle = 6.28318530717958647692f * u2;
        const float z0 = radius * std::cos(angle);
        return mean + z0 * stddev;
    }

    float signedUnit() noexcept
    {
        return unitFloat() * 2.0f - 1.0f;
    }

    float signedRange(float magnitude) noexcept
    {
        return signedUnit() * magnitude;
    }

    float2 jitter2D(float magnitude) noexcept
    {
        return {signedRange(magnitude), signedRange(magnitude)};
    }

    float3 jitter3D(float magnitude) noexcept
    {
        return {signedRange(magnitude), signedRange(magnitude), signedRange(magnitude)};
    }

    template <typename T>
    const T& pick(const std::vector<T>& values) noexcept
    {
        return values[static_cast<size_t>(rangeInclusive(0, static_cast<int>(values.size()) - 1))];
    }

    template <typename T, size_t N>
    const T& pick(const std::array<T, N>& values) noexcept
    {
        return values[static_cast<size_t>(rangeInclusive(0, static_cast<int>(N) - 1))];
    }

    template <typename T>
    void shuffle(std::vector<T>& values) noexcept
    {
        if (values.size() < 2) {
            return;
        }
        for (size_t i = values.size() - 1; i > 0; --i) {
            const size_t j = static_cast<size_t>(rangeInclusive(0, static_cast<int>(i)));
            if (i != j) {
                std::swap(values[i], values[j]);
            }
        }
    }

    RandomStream fork(uint64_t salt) const noexcept
    {
        RandomStream child{splitmix64(state_ ^ mix(salt))};
        return child;
    }

    static uint64_t mix(uint64_t value) noexcept
    {
        return splitmix64(value + kStreamMix);
    }

    static uint64_t defaultSeed() noexcept
    {
        return kDefaultSeed;
    }

private:
    static constexpr uint64_t kDefaultSeed = 0x9E3779B97F4A7C15ull;
    static constexpr uint64_t kGamma = 0x9E3779B97F4A7C15ull;
    static constexpr uint64_t kStreamMix = 0xBF58476D1CE4E5B9ull;
    static constexpr float kUnitFloat = 1.0f / 16777216.0f;
    static constexpr double kUnitDouble = 1.0 / 9007199254740992.0;

    static uint64_t splitmix64(uint64_t value) noexcept
    {
        value += 0x9E3779B97F4A7C15ull;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
        return value ^ (value >> 31);
    }

    uint64_t state_ = kDefaultSeed;
};

class Random {
public:
    static Random& instance()
    {
        static Random rng;
        return rng;
    }

    void seed(uint64_t seedValue)
    {
        stream_.seed(seedValue);
    }

    uint64_t nextU64()
    {
        return stream_.nextU64();
    }

    float float01()
    {
        return stream_.unitFloat();
    }

    float floatRange(float minValue, float maxValue)
    {
        return stream_.range(minValue, maxValue);
    }

    int intRange(int minValue, int maxValue)
    {
        return stream_.rangeInclusive(minValue, maxValue);
    }

    bool coin(float probability = 0.5f)
    {
        return stream_.chance(probability);
    }

    float gaussian()
    {
        return stream_.gaussian();
    }

    float gaussian(float mean, float stddev)
    {
        return stream_.gaussian(mean, stddev);
    }

    float2 jitter2D(float magnitude)
    {
        return stream_.jitter2D(magnitude);
    }

    float3 jitter3D(float magnitude)
    {
        return stream_.jitter3D(magnitude);
    }

    template <typename T>
    const T& pick(const std::vector<T>& values)
    {
        return stream_.pick(values);
    }

    template <typename T, size_t N>
    const T& pick(const std::array<T, N>& values)
    {
        return stream_.pick(values);
    }

    template <typename T>
    void shuffle(std::vector<T>& values)
    {
        stream_.shuffle(values);
    }

    RandomStream stream() const
    {
        return stream_;
    }

private:
    Random()
        : stream_(entropySeed())
    {
    }

    RandomStream stream_;

    static uint64_t entropySeed()
    {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    }
};

inline RandomStream makeRandomStream(uint64_t seed)
{
    return RandomStream(seed);
}

inline float randomFloat01()
{
    return Random::instance().float01();
}

inline float randomFloatRange(float minValue, float maxValue)
{
    return Random::instance().floatRange(minValue, maxValue);
}

inline int randomIntRange(int minValue, int maxValue)
{
    return Random::instance().intRange(minValue, maxValue);
}

inline float randomGaussian()
{
    return Random::instance().gaussian();
}

} // namespace ArtifactCore
