module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:TiltShift;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct TiltShiftSettings {
    float focusPos = 0.5f;     // Normalized vertical focus line (0.0 to 1.0)
    float focusWidth = 0.2f;   // Symmetrical width of the sharp area (0.0 to 1.0)
    float blurFalloff = 0.15f; // Transition smoothness to full blur (0.0 to 1.0)
    float maxBlur = 1.0f;      // Max blur blend opacity (0.0 to 1.0)
};

class LIBRARY_DLL_API TiltShift {
public:
    TiltShift() = default;
    ~TiltShift() = default;

    // Process a raw float4 (RGBA) buffer
    void process(float4* buffer, int width, int height, const TiltShiftSettings& settings);

    // Process an ImageF32x4_RGBA reference
    void process(ImageF32x4_RGBA& image, const TiltShiftSettings& settings);
};

} // namespace ArtifactCore
