module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:AnisotropicFlowBlur;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct AnisotropicFlowBlurSettings {
    // Overall blur radius along the edge flow direction
    float blurAmount = 10.0f;

    // Noise suppression scale for structure tensor
    float tensorNoiseScale = 1.0f;

    // Local integration scale for neighborhood consensus flow direction
    float tensorIntegrationScale = 4.0f;

    // High flow adherence (0.0 = regular blur, 1.0 = strictly along edges)
    float edgeAdherence = 0.8f;
};

class LIBRARY_DLL_API AnisotropicFlowBlur {
public:
    AnisotropicFlowBlur() = default;
    ~AnisotropicFlowBlur() = default;

    // Apply AnisotropicFlowBlur directly to the image
    void process(ImageF32x4_RGBA& image, const AnisotropicFlowBlurSettings& settings);
};

} // namespace ArtifactCore
