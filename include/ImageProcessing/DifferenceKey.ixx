module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:DifferenceKey;

import Graphics.SurfaceColorContract;

export namespace ArtifactCore::Keying {

// Difference keyer: RGB Euclidean distance to a reference color + edge
// finishing + matte view. Alpha semantics follow the user-facing effect:
// alpha = (distance - threshold) / softness, clamped.
struct DifferenceKeyParams {
    float refR = 0.0f;
    float refG = 0.0f;
    float refB = 0.0f;
    float threshold = 0.1f;
    float softness = 0.08f;
    float choke = 0.0f; // -1..1 (negative=dilate, positive=erode), ~3px max
    float matteBlur = 0.0f; // 0..2, 3x3 box blend factor source
    int viewMode = 0; // 0=Final, 1=Matte
};

struct DifferenceKeyBuffers {
    const float* src = nullptr;
    float* dst = nullptr;
    int width = 0;
    int height = 0;
    SurfaceChannelOrder channelOrder = SurfaceChannelOrder::RGBA;
    SurfaceAlphaMode alphaMode = SurfaceAlphaMode::Straight;
};

// src and dst must be distinct tightly packed RGBA/BGRA buffers in channelOrder.
// Premultiplied RGB is unassociated for distance evaluation and associated
// again using the keyed output alpha. Other alpha modes use straight RGB storage.
// Returns false for invalid order, dimensions, or missing buffers.
bool processDifferenceKey(const DifferenceKeyBuffers& buffers,
                          const DifferenceKeyParams& params = {});

} // namespace ArtifactCore::Keying
