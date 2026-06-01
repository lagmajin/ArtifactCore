module;
#include "../Define/DllExportMacro.hpp"
#include <memory>

export module ImageProcessing:EdgeEcho;

import Particle;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct EdgeEchoSettings {
    float edgeThreshold = 0.15f; // Sobel edge cutoff (0.0 to 1.0)
    float decay = 0.85f;          // Echo trail persistence decay (0.0 to 1.0)
    float waveFreq = 8.0f;       // Horizontal wave distortion frequency
    float waveAmp = 5.0f;        // Wave distortion amplitude in pixels
    float timeEvolution = 0.0f;   // Time/evolution parameter to animate wave
    float4 echoColor{0.0f, 1.0f, 0.4f, 1.0f}; // Neon echo outline color (default: electric green)
};

class LIBRARY_DLL_API EdgeEcho {
public:
    EdgeEcho();
    ~EdgeEcho();

    // Process a raw float4 (RGBA) buffer
    void process(float4* buffer, int width, int height, const EdgeEchoSettings& settings);

    // Process an ImageF32x4_RGBA reference
    void process(ImageF32x4_RGBA& image, const EdgeEchoSettings& settings);

    // Reset outline history buffer
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
