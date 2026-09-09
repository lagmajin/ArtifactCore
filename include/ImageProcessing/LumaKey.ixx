module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:LumaKey;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore::Keying {

// Modern range keyer: dual-threshold luma matte + edge finishing + views.
// Alpha semantics follow the user-facing effect: [low, high] fully opaque,
// feathered outside (alpha = min((luma-low)/soft, (high-luma)/soft)).
struct LumaKeyParams {
    float low = 0.15f;
    float high = 0.85f;
    float softness = 0.08f;
    float choke = 0.0f; // -1..1 (negative=dilate, positive=erode), ~3px max
    float matteBlur = 0.0f; // 0..2, 3x3 box blend factor source
    int viewMode = 0; // 0=Final, 1=Matte
};

struct LumaKeyBuffers {
    const float* src = nullptr;
    float* dst = nullptr;
    int width = 0;
    int height = 0;
};

// src and dst must be distinct buffers. Returns false for invalid
// dimensions, missing buffers, or (viewMode aside) nothing to do.
bool processLumaKey(const LumaKeyBuffers& buffers,
                    const LumaKeyParams& params = {});

} // namespace ArtifactCore::Keying

export namespace ArtifactCore {

struct LumaKeySettings {
    float lowThreshold = 0.0f;
    float highThreshold = 1.0f;
    float softness = 0.05f;
};

class LIBRARY_DLL_API LumaKey {
public:
    LumaKey() = default;

    void process(float4* buffer, int width, int height, const LumaKeySettings& settings);
    void process(ImageF32x4_RGBA& image, const LumaKeySettings& settings);
};

}
