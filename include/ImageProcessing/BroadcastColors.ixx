module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:BroadcastColors;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

enum class BroadcastStandard { NTSC, PAL };

struct BroadcastColorsSettings {
    BroadcastStandard standard = BroadcastStandard::NTSC;
    float reduceSaturation = 0.5f;
};

class LIBRARY_DLL_API BroadcastColors {
public:
    BroadcastColors() = default;
    void process(float4* buffer, int width, int height, const BroadcastColorsSettings& settings);
    void process(ImageF32x4_RGBA& image, const BroadcastColorsSettings& settings);
};

}
