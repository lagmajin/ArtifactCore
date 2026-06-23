module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:LeaveColor;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct LeaveColorSettings {
    float4 keyColor = float4{0, 1, 0, 1};
    float tolerance = 0.2f;
    float softness = 0.1f;
    float desaturateAmount = 1.0f;
};

class LIBRARY_DLL_API LeaveColor {
public:
    LeaveColor() = default;
    void process(float4* buffer, int width, int height, const LeaveColorSettings& settings);
    void process(ImageF32x4_RGBA& image, const LeaveColorSettings& settings);
};

}
