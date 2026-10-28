module;
#include <utility>
#include <algorithm>
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <random>
#include <mutex>

export module Math.Random;

export import Math.RandomStream;

export namespace ArtifactCore {

class Random {
public:
    static Random& instance()
    {
        static Random rng;
        return rng;
    }

    // All methods are mutex-guarded: the global stream is shared across
    // threads by design. Use forked RandomStream instances for lock-free
    // per-thread determinism.
    void seed(uint64_t seedValue)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stream_.seed(seedValue);
    }

    uint64_t nextU64()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.nextU64();
    }

    float float01()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.unitFloat();
    }

    float floatRange(float minValue, float maxValue)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.range(minValue, maxValue);
    }

    int intRange(int minValue, int maxValue)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.rangeInclusive(minValue, maxValue);
    }

    bool coin(float probability = 0.5f)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.chance(probability);
    }

    float gaussian()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.gaussian();
    }

    float gaussian(float mean, float stddev)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.gaussian(mean, stddev);
    }

    float2 jitter2D(float magnitude)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.jitter2D(magnitude);
    }

    float3 jitter3D(float magnitude)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.jitter3D(magnitude);
    }

    template <typename T>
    const T& pick(const std::vector<T>& values)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.pick(values);
    }

    template <typename T, size_t N>
    const T& pick(const std::array<T, N>& values)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_.pick(values);
    }

    template <typename T>
    void shuffle(std::vector<T>& values)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stream_.shuffle(values);
    }

    RandomStream stream() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_;
    }

private:
    Random()
        : stream_(entropySeed())
    {
    }

    RandomStream stream_;
    mutable std::mutex mutex_;

    static uint64_t entropySeed()
    {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    }
};

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
