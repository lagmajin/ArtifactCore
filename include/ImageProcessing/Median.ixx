module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Median;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct MedianSettings {
    int radius = 1;
};

class LIBRARY_DLL_API Median {
public:
    Median() = default;

    void process(float4* buffer, int width, int height, const MedianSettings& settings);
    void process(ImageF32x4_RGBA& image, const MedianSettings& settings);
};

}
