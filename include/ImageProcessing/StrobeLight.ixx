module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:StrobeLight;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct StrobeLightSettings {
    float frequency = 1.0f;
    float duration = 0.5f;
    float4 color = float4{1.0f, 1.0f, 1.0f, 1.0f};
    float blendWithOriginal = 0.0f;
};

class LIBRARY_DLL_API StrobeLight {
public:
    StrobeLight();
    void process(float4* buffer, int width, int height, const StrobeLightSettings& settings);
    void process(ImageF32x4_RGBA& image, const StrobeLightSettings& settings);
    void reset();

private:
    int frameCounter_ = 0;
};

}
