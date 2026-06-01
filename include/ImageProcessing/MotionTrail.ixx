module;
#include "../Define/DllExportMacro.hpp"
#include <memory>

export module ImageProcessing:MotionTrail;

import Particle; // For float4 definition
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

enum class MotionTrailMode {
    Blend,
    Additive,
    Maximum
};

struct MotionTrailSettings {
    float decay = 0.8f;       // Rate at which previous frames fade (0.0 to 1.0)
    float intensity = 1.0f;   // Overall opacity of the effect (0.0 to 1.0)
    MotionTrailMode mode = MotionTrailMode::Blend;
};

class LIBRARY_DLL_API MotionTrail {
public:
    MotionTrail();
    ~MotionTrail();

    // Process a raw float4 (RGBA) buffer
    void process(float4* buffer, int width, int height, const MotionTrailSettings& settings);

    // Process an ImageF32x4_RGBA image reference (convenience wrapper)
    void process(ImageF32x4_RGBA& image, const MotionTrailSettings& settings);

    // Reset the history buffer manually
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
