module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>

export module ImageProcessing:WireRemove;

export namespace ArtifactCore::Repair {

// One wire segment in normalized coordinates. Negative/overscan values are
// allowed so tracked endpoints can leave the frame.
struct WireSegment {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    bool enabled = false;
};

struct WireRemoveParams {
    WireSegment segs[3];
    float width = 8.0f;
    float feather = 5.0f;
    float cloneOffsetX = 32.0f;
    float cloneOffsetY = 0.0f;
    float mix = 1.0f;
    float inpaintRadius = 4.0f;
    int method = 0; // 0=SideAverage 1=CloneOffset 2=Blur 3=Telea 4=NS
    bool viewMask = false;
};

struct WireRemoveBuffers {
    const float* src = nullptr;
    float* dst = nullptr;
    // Optional replacement (clean plate / temporal neighbor), same size as
    // src, or nullptr. Takes priority over the procedural fill methods.
    const float* clean = nullptr;
    // Optional pre-built removal mask (0..1 float plane), or nullptr.
    const float* namedMask = nullptr;
    int width = 0;
    int height = 0;
};

// Full CPU pipeline: segment mask + named mask, then clean/fill/inpaint
// composite. src and dst must be distinct buffers. Returns false for invalid
// dimensions, missing buffers, or an empty removal mask (dst untouched).
bool processWireRemove(const WireRemoveBuffers& buffers,
                       const WireRemoveParams& params = {});

} // namespace ArtifactCore::Repair
