module;
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:PosterizeTime;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct PosterizeTimeSettings {
    float frameRate = 12.0f;
};

class LIBRARY_DLL_API PosterizeTime {
public:
    PosterizeTime();
    ~PosterizeTime();
    PosterizeTime(const PosterizeTime&) = delete;
    PosterizeTime& operator=(const PosterizeTime&) = delete;
    void process(float4* buffer, int width, int height, const PosterizeTimeSettings& settings);
    void process(ImageF32x4_RGBA& image, const PosterizeTimeSettings& settings);
    void reset();

private:
    int frameCounter_ = 0;
    float4* heldBuffer_ = nullptr;
    int heldW_ = 0, heldH_ = 0;
};

}
