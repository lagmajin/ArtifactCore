module;
#include <algorithm>
#include <cmath>

module ImageProcessing:StrobeLight;

namespace ArtifactCore {

StrobeLight::StrobeLight() : frameCounter_(0) {}

void StrobeLight::reset() { frameCounter_ = 0; }

void StrobeLight::process(float4* buffer, int width, int height, const StrobeLightSettings& s) {
    int framesPerCycle = std::max(1, static_cast<int>(std::round(30.0f / s.frequency)));
    int framesOn = std::max(1, static_cast<int>(framesPerCycle * s.duration));
    bool strobeOn = (frameCounter_ % framesPerCycle) < framesOn;
    if (strobeOn) {
        for (int i = 0; i < width * height; ++i) {
            auto& px = buffer[i];
            px.x = px.x * s.blendWithOriginal + s.color.x * (1.0f - s.blendWithOriginal);
            px.y = px.y * s.blendWithOriginal + s.color.y * (1.0f - s.blendWithOriginal);
            px.z = px.z * s.blendWithOriginal + s.color.z * (1.0f - s.blendWithOriginal);
            px.w = px.w * s.blendWithOriginal + s.color.w * (1.0f - s.blendWithOriginal);
        }
    }
    ++frameCounter_;
}

void StrobeLight::process(ImageF32x4_RGBA& image, const StrobeLightSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
