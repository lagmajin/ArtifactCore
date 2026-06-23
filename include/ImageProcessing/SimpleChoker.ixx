module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:SimpleChoker;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct SimpleChokerSettings {
    float choke = 0.5f;
    int radius = 2;
};

class LIBRARY_DLL_API SimpleChoker {
public:
    SimpleChoker() = default;
    void process(float4* buffer, int width, int height, const SimpleChokerSettings& settings);
    void process(ImageF32x4_RGBA& image, const SimpleChokerSettings& settings);
};

}
