module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:ChromaKey;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct ChromaKeySettings {
    float4 keyColor = float4{0, 1, 0, 1};
    float tolerance = 0.2f;
    float softness = 0.1f;
};

class LIBRARY_DLL_API ChromaKey {
public:
    ChromaKey() = default;
    void process(float4* buffer, int width, int height, const ChromaKeySettings& settings);
    void process(ImageF32x4_RGBA& image, const ChromaKeySettings& settings);
};

}
