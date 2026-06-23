module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:LumaKey;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct LumaKeySettings {
    float lowThreshold = 0.0f;
    float highThreshold = 1.0f;
    float softness = 0.05f;
};

class LIBRARY_DLL_API LumaKey {
public:
    LumaKey() = default;

    void process(float4* buffer, int width, int height, const LumaKeySettings& settings);
    void process(ImageF32x4_RGBA& image, const LumaKeySettings& settings);
};

}
