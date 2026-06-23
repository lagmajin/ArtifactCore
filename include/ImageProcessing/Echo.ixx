module;
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module ImageProcessing:Echo;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct EchoSettings {
    int echoCount = 3;
    float decay = 0.5f;
    float startingIntensity = 1.0f;
};

class LIBRARY_DLL_API Echo {
public:
    Echo();
    ~Echo();
    Echo(const Echo&) = delete;
    Echo& operator=(const Echo&) = delete;
    void process(float4* buffer, int width, int height, const EchoSettings& settings);
    void process(ImageF32x4_RGBA& image, const EchoSettings& settings);
    void reset();

private:
    int writePos_ = 0;
    int frameCount_ = 0;
    std::vector<float4> ringBuffer_;
    int ringCapacity_ = 0;
    int bufW_ = 0, bufH_ = 0;
};

}
