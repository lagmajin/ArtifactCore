module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Threshold;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct ThresholdSettings {
    float threshold = 0.5f;
    float softness = 0.0f;
};

class LIBRARY_DLL_API Threshold {
public:
    Threshold() = default;

    void process(float4* buffer, int width, int height, const ThresholdSettings& settings);
    void process(ImageF32x4_RGBA& image, const ThresholdSettings& settings);
};

}
