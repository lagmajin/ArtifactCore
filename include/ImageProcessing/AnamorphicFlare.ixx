module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:AnamorphicFlare;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct AnamorphicFlareSettings {
    float threshold = 0.8f;   // Brightness threshold for light sources (0.0 to 1.0)
    float flareLength = 0.4f;  // Width/spread of the horizontal flare (0.0 to 1.0)
    float intensity = 1.0f;    // Strength multiplier of the flare (0.0 to 5.0)
    float4 tint{0.1f, 0.4f, 1.0f, 1.0f}; // Color tint (default is beautiful cinematic cyan/blue)
};

class LIBRARY_DLL_API AnamorphicFlare {
public:
    AnamorphicFlare() = default;
    ~AnamorphicFlare() = default;

    // Process a raw float4 (RGBA) buffer
    void process(float4* buffer, int width, int height, const AnamorphicFlareSettings& settings);

    // Process an ImageF32x4_RGBA reference
    void process(ImageF32x4_RGBA& image, const AnamorphicFlareSettings& settings);
};

} // namespace ArtifactCore
