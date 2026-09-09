module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>

export module ImageProcessing:ChromaKey;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore::Keying {

// Keylight-style classical chroma keyer parameters. Breaking reorg of the
// legacy Euclidean-RGB ChromaKeySettings: YCbCr chroma distance + screen
// matte clip + despill + edge finishing + matte preview.
struct ChromaKeyParams {
    float4 keyColor = float4{0, 1, 0, 1};
    float hueTolerance = 0.28f;
    float edgeSoftness = 0.12f;
    float clipBlack = 0.0f;
    float clipWhite = 1.0f;
    float despillStrength = 0.7f;
    int despillMode = 1; // 0=Off, 1=ChannelSuppress, 2=LuminancePreserve
    float choke = 0.0f; // -1..1 (negative=dilate, positive=erode), ~3px max
    float matteBlur = 0.0f; // 0..2, 3x3 box blend factor source
    int viewMode = 0; // 0=Final, 1=ScreenMatte, 2=DespillMap, 3=Status
};

// Must match the ChromaKey HLSL constant buffer layout (16-byte aligned).
struct ChromaKeyGpuParams {
    float keyR = 0.0f;
    float keyG = 1.0f;
    float keyB = 0.0f;
    float pad0 = 0.0f;
    float hueTolerance = 0.28f;
    float edgeSoftness = 0.12f;
    float clipBlack = 0.0f;
    float clipWhite = 1.0f;
    float despillStrength = 0.7f;
    std::int32_t despillMode = 1;
    std::int32_t viewMode = 0;
    float choke = 0.0f;
    float matteBlur = 0.0f;
    float pad1 = 0.0f;
    float pad2 = 0.0f;
    float pad3 = 0.0f;

    static ChromaKeyGpuParams fromParams(const ChromaKeyParams& source);
};

static_assert(sizeof(ChromaKeyGpuParams) == 64,
              "ChromaKeyGpuParams must match the ChromaKey HLSL constant buffer.");

struct ChromaKeyBuffers {
    const float* src = nullptr;
    float* dst = nullptr;
    int width = 0;
    int height = 0;
};

// Processes tightly packed RGBA float32 buffers. Returns false for invalid
// dimensions or missing buffers; otherwise dst is fully written.
bool processChromaKey(const ChromaKeyBuffers& buffers, const ChromaKeyParams& params = {});

} // namespace ArtifactCore::Keying

export namespace ArtifactCore {

// Legacy Euclidean-RGB settings kept as a deprecated alias so existing
// callers keep compiling; new code should use Keying::ChromaKeyParams.
struct ChromaKeySettings {
    float4 keyColor = float4{0, 1, 0, 1};
    float tolerance = 0.2f;
    float softness = 0.1f;
};

class LIBRARY_DLL_API ChromaKey {
public:
    ChromaKey() = default;
    void process(float4* buffer, int width, int height, const ChromaKeySettings& settings);
    void process(ImageF32x4_RGBA& image, const ChromaKeySettings& settings);
};

}
