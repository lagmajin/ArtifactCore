module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:ChromaSpreadGlow;

import Image.ImageF32x4_RGBA;
import Particle;

export namespace ArtifactCore {

struct ChromaSpreadGlowSettings {
    // Luminance threshold for source selection (0.0 to 1.0)
    float threshold = 0.5f;

    // Radius of the glow bloom
    float glowRadius = 20.0f;

    // Intensity multiplier for the composited glow (additive)
    float intensity = 1.0f;

    // Chromatic dispersion scale multiplier (1.0 = normal, higher = more separation)
    float aberrationScale = 1.02f;

    // Wavelength interpolation steps (for extra smooth rainbow glow)
    int dispersionSteps = 5;

    // Optional tint color for the glow
    float4 tintColor{1.0f, 1.0f, 1.0f, 1.0f};
};

class LIBRARY_DLL_API ChromaSpreadGlow {
public:
    ChromaSpreadGlow() = default;
    ~ChromaSpreadGlow() = default;

    // Apply ChromaSpreadGlow directly to the image
    void process(ImageF32x4_RGBA& image, const ChromaSpreadGlowSettings& settings);
};

} // namespace ArtifactCore
