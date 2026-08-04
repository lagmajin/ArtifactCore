module;
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <vector>

export module ImageProcessing:IBKKeyer;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore::Keying {

struct IBKParams {
    float screenCorrection = 1.0f;
    float coreMatteClip = 0.5f;
    float edgeMatteSoftness = 0.2f;
    float despillStrength = 0.5f;
    float garbageMatteGamma = 1.0f;
    float detailRecovery = 0.3f;
    int erodePixels = 1;
    int dilatePixels = 3;
};

struct IBKGpuParams {
    float screenCorrection = 1.0f;
    float coreMatteClip = 0.5f;
    float edgeMatteSoftness = 0.2f;
    float despillStrength = 0.5f;
    float garbageMatteGamma = 1.0f;
    float detailRecovery = 0.3f;
    std::uint32_t erodePixels = 1;
    std::uint32_t dilatePixels = 3;

    static IBKGpuParams fromParams(const IBKParams& source) {
        const auto finiteOr = [](float value, float fallback) {
            return std::isfinite(value) ? value : fallback;
        };
        IBKGpuParams result;
        result.screenCorrection = std::clamp(
            finiteOr(source.screenCorrection, 1.0f), 0.0f, 4.0f);
        result.coreMatteClip = std::clamp(
            finiteOr(source.coreMatteClip, 0.5f), 0.0f, 1.0f);
        result.edgeMatteSoftness = std::clamp(
            finiteOr(source.edgeMatteSoftness, 0.2f), 1.0e-5f, 1.0f);
        result.despillStrength = std::clamp(
            finiteOr(source.despillStrength, 0.5f), 0.0f, 1.0f);
        result.garbageMatteGamma = std::clamp(
            finiteOr(source.garbageMatteGamma, 1.0f), 1.0e-5f, 4.0f);
        result.detailRecovery = std::clamp(
            finiteOr(source.detailRecovery, 0.3f), 0.0f, 1.0f);
        result.erodePixels = static_cast<std::uint32_t>(
            std::clamp(source.erodePixels, 0, 64));
        result.dilatePixels = static_cast<std::uint32_t>(
            std::clamp(source.dilatePixels, 0, 64));
        return result;
    }
};

static_assert(sizeof(IBKGpuParams) == 32,
              "IBKGpuParams must match the IBK HLSL constant buffer.");

struct IBKBuffers {
    const float* foreground = nullptr;
    const float* cleanPlate = nullptr;
    float* outputRGBA = nullptr;
    int width = 0;
    int height = 0;
};

// Processes tightly packed RGBA float32 buffers. Returns false for invalid
// dimensions or missing buffers; otherwise outputRGBA is fully written.
bool processIBK(const IBKBuffers& buffers, const IBKParams& params = {});

// Builds a clean plate by taking the per-channel temporal median. All input
// frames must have identical, non-zero dimensions.
bool autoGenerateCleanPlate(const std::vector<ImageF32x4_RGBA>& frames,
                            ImageF32x4_RGBA& outCleanPlate);

} // namespace ArtifactCore::Keying
