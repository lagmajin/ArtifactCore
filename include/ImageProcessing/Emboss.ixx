module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Emboss;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct EmbossSettings {
    float intensity = 1.0f;
    float angle = 0.0f;
};

class LIBRARY_DLL_API Emboss {
public:
    Emboss() = default;
    void process(float4* buffer, int width, int height, const EmbossSettings& settings);
    void process(ImageF32x4_RGBA& image, const EmbossSettings& settings);
};

}
