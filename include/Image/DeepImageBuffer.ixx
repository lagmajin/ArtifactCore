module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

export module Image.DeepImageBuffer;

export namespace ArtifactCore {

struct DeepSample {
    float depth = 0.0f;
    float depthBack = 0.0f; // optional back depth; <= depth means a point sample
    std::array<float, 4> color{0.0f, 0.0f, 0.0f, 0.0f};
    float alpha = 0.0f;
    float coverage = 1.0f;
    bool holdout = false;
};

struct DeepPixel {
    std::vector<DeepSample> samples;
};

struct DeepSampleGpu {
    float depth = 0.0f;
    float depthBack = 0.0f;
    std::array<float, 4> color{0.0f, 0.0f, 0.0f, 0.0f};
    float alpha = 0.0f;
    float coverage = 1.0f;
    std::uint32_t holdout = 0;
    std::uint32_t padding = 0;
};

static_assert(sizeof(DeepSampleGpu) == 40,
              "DeepSampleGpu must match the HLSL StructuredBuffer stride.");

struct DeepImagePacked {
    std::vector<DeepSample> flatSamples;
    std::vector<DeepSampleGpu> gpuSamples;
    std::vector<std::uint32_t> sampleOffsets;
    std::vector<std::uint16_t> sampleCounts;
    std::vector<std::uint32_t> gpuSampleCounts;
    std::size_t totalSamples = 0;
    int width = 0;
    int height = 0;

    bool isValid() const
    {
        if (width <= 0 || height <= 0 ||
            static_cast<std::size_t>(width) >
                static_cast<std::size_t>(-1) / static_cast<std::size_t>(height)) {
            return false;
        }
        const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                       static_cast<std::size_t>(height);
        return width > 0 && height > 0 &&
               sampleOffsets.size() == sampleCounts.size() &&
               sampleOffsets.size() == gpuSampleCounts.size() &&
               sampleOffsets.size() == pixelCount &&
               totalSamples == flatSamples.size() &&
               gpuSamples.size() == flatSamples.size();
    }

    bool buffersAreConsistent() const
    {
        if (!isValid()) return false;
        std::size_t previousEnd = 0;
        for (std::size_t i = 0; i < sampleOffsets.size(); ++i) {
            const std::size_t offset = sampleOffsets[i];
            const std::size_t count = gpuSampleCounts[i];
            if (count != sampleCounts[i] || offset < previousEnd ||
                offset > flatSamples.size() ||
                count > flatSamples.size() - offset) {
                return false;
            }
            previousEnd = offset + count;
        }
        if (previousEnd != flatSamples.size()) return false;
        for (std::size_t i = 0; i < flatSamples.size(); ++i) {
            const auto& cpu = flatSamples[i];
            const auto& gpu = gpuSamples[i];
            if (cpu.depth != gpu.depth || cpu.depthBack != gpu.depthBack ||
                cpu.color != gpu.color || cpu.alpha != gpu.alpha ||
                cpu.coverage != gpu.coverage ||
                (cpu.holdout ? 1u : 0u) != gpu.holdout) {
                return false;
            }
        }
        return true;
    }

    static constexpr std::size_t gpuSampleStrideBytes() noexcept
    {
        return sizeof(DeepSampleGpu);
    }
};

class DeepImageBuffer {
public:
    DeepImageBuffer() = default;
    DeepImageBuffer(int width, int height);

    bool resize(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    bool isEmpty() const { return width_ <= 0 || height_ <= 0 || pixels_.empty(); }
    std::size_t totalSampleCount() const;
    std::size_t approximateMemoryBytes() const;

    DeepPixel* pixel(int x, int y);
    const DeepPixel* pixel(int x, int y) const;
    bool addSample(int x, int y, const DeepSample& sample);
    // Validates and clamps all samples, then restores front-to-back order.
    bool normalizeSamples();
    void sortSamplesByDepth();
    bool clipDepthRange(float nearDepth, float farDepth);
    void prune(float minimumAlpha = 1.0e-5f, std::size_t maxSamplesPerPixel = 0);

    static DeepImageBuffer fromFlatRGBA(const float* rgba, int width, int height);
    static DeepImageBuffer fromFlatRGBAWithDepth(const float* rgba, const float* depth,
                                                 int width, int height);
    bool addFlatRGBAAtDepth(const float* rgba, int width, int height, float depth);
    bool toFlatRGBA(float* rgba, std::size_t floatCount) const;
    // Creates a normalized alpha matte from samples within the inclusive depth range.
    bool toDepthMatteRGBA(float* rgba, std::size_t floatCount,
                          float nearDepth, float farDepth) const;
    // Flattens the image with a depth-dependent CPU defocus pass.
    bool toDepthOfFieldRGBA(float* rgba, std::size_t floatCount,
                            float focalDepth, float focusRange,
                            int maxBlurRadius = 8) const;
    // Exports one depth-ranked coverage layer per pixel without flattening
    // samples from other ranks into it. Call sortSamplesByDepth() first;
    // rank zero is then the nearest sample.
    bool toRankedRGBA(float* rgba, std::size_t floatCount, std::size_t rank) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<DeepPixel> pixels_;
};

// Combines samples from two equal-sized buffers and keeps front-to-back depth order.
bool mergeDeepOver(const DeepImageBuffer& front,
                   const DeepImageBuffer& back,
                   DeepImageBuffer& result,
                   std::size_t maxSamplesPerPixel = 0);

// Applies a holdout buffer to the target without flattening either buffer.
bool applyDeepHoldout(const DeepImageBuffer& holdout,
                      DeepImageBuffer& target);
bool packDeepImage(const DeepImageBuffer& image, DeepImagePacked& packed);
bool unpackDeepImage(const DeepImagePacked& packed, DeepImageBuffer& image);

// Composites a flat RGBA image as a single deep sample per pixel.
bool compositeFlatOverDeep(const float* rgba, int width, int height, float depth,
                           DeepImageBuffer& target,
                           std::size_t maxSamplesPerPixel = 0);
bool compositeDeepOverFlat(const DeepImageBuffer& source, const float* rgba,
                           std::size_t floatCount, float depth,
                           float* output);

} // namespace ArtifactCore
