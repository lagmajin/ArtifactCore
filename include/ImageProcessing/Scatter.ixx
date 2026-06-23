module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Scatter;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct ScatterSettings {
    float amount = 10.0f;
    int seed = 1;
};

class LIBRARY_DLL_API Scatter {
public:
    Scatter() = default;
    void process(float4* buffer, int width, int height, const ScatterSettings& settings);
    void process(ImageF32x4_RGBA& image, const ScatterSettings& settings);
};

}
