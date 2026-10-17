module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Duotone;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct DuotoneSettings {
    float4 shadowColor{0.05f, 0.1f, 0.25f, 1.0f};      // Logical RGBA color for dark areas.
    float4 highlightColor{0.95f, 0.85f, 0.6f, 1.0f};  // Logical RGBA color for light areas.
    float blend = 1.0f;                                // Intensity blend factor (0.0 to 1.0)
};

class LIBRARY_DLL_API Duotone {
public:
    Duotone() = default;
    ~Duotone() = default;

    // Process a raw float4 buffer in Artifact BGRA backing order.
    void process(float4* buffer, int width, int height, const DuotoneSettings& settings);

    // Process an ImageF32x4_RGBA reference
    void process(ImageF32x4_RGBA& image, const DuotoneSettings& settings);
};

} // namespace ArtifactCore
