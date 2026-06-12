module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:VectorFlowGlitch;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct VectorFlowGlitchSettings {
    // Overall displacement offset in pixels
    float glitchAmount = 20.0f;

    // Slicing block frequency (higher values mean thinner slice blocks)
    float frequency = 0.05f;

    // RGB channel tearing separation intensity (chromatic aberration)
    float chromaticAberration = 5.0f;

    // Influence of local structure tensor (0.0 = horizontal glitch, 1.0 = strictly along edges)
    float edgeFlowInfluence = 0.7f;

    // Time/Evolution parameter to animate the glitch
    float seed = 0.0f;
};

class LIBRARY_DLL_API VectorFlowGlitch {
public:
    VectorFlowGlitch() = default;
    ~VectorFlowGlitch() = default;

    // Apply VectorFlowGlitch directly to the image
    void process(ImageF32x4_RGBA& image, const VectorFlowGlitchSettings& settings);
};

} // namespace ArtifactCore
