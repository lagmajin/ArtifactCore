module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Halftone;

import Particle; // For float4 definition
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct HalftoneSettings {
    float dotSize = 8.0f;     // Grid size / dot spacing (pixels)
    float angle = 0.0f;       // Rotation of grid (degrees)
    float contrast = 1.0f;    // Dot threshold contrast scaler
    bool isColor = false;     // True: colorful halftone, False: monochrome (black & white)
};

class LIBRARY_DLL_API Halftone {
public:
    Halftone() = default;
    ~Halftone() = default;

    // Process a raw float4 (RGBA) buffer
    void process(float4* buffer, int width, int height, const HalftoneSettings& settings);

    // Process an ImageF32x4_RGBA reference
    void process(ImageF32x4_RGBA& image, const HalftoneSettings& settings);
};

} // namespace ArtifactCore
