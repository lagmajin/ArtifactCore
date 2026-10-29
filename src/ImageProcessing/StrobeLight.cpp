module;
#include <algorithm>
#include <cmath>
#include <limits>

module ImageProcessing;
import :StrobeLight;
import Core.Parallel;

namespace ArtifactCore {

StrobeLight::StrobeLight() : frameCounter_(0) {}

void StrobeLight::reset() { frameCounter_ = 0; }

void StrobeLight::process(float4* buffer, int width, int height, const StrobeLightSettings& s) {
    if (!buffer || width <= 0 || height <= 0) return;

    int framesPerCycle = 1;
    if (std::isfinite(s.frequency) && s.frequency > 0.0f) {
        const double roundedPeriod = std::round(30.0 / static_cast<double>(s.frequency));
        framesPerCycle = static_cast<int>(std::clamp(roundedPeriod, 1.0,
            static_cast<double>(std::numeric_limits<int>::max())));
    }
    const float duration = std::isfinite(s.duration)
        ? std::clamp(s.duration, 0.0f, 1.0f) : 0.0f;
    const int framesOn = duration <= 0.0f ? 0 : std::max(1,
        static_cast<int>(static_cast<double>(framesPerCycle) * duration));
    const float blend = std::isfinite(s.blendWithOriginal)
        ? std::clamp(s.blendWithOriginal, 0.0f, 1.0f) : 0.0f;
    bool strobeOn = (frameCounter_ % framesPerCycle) < framesOn;
    if (strobeOn) {
        Parallel::ForTiles(width, height, 64, 64, [&](int x0, int y0, int x1, int y1) {
            for (int y = y0; y < y1; ++y) {
                const size_t rowStart = static_cast<size_t>(y) * static_cast<size_t>(width);
                for (int x = x0; x < x1; ++x) {
                    auto& px = buffer[rowStart + static_cast<size_t>(x)];
                    px.x = px.x * blend + s.color.z * (1.0f - blend);
                    px.y = px.y * blend + s.color.y * (1.0f - blend);
                    px.z = px.z * blend + s.color.x * (1.0f - blend);
                    px.w = px.w * blend + s.color.w * (1.0f - blend);
                }
            }
        });
    }
    frameCounter_ = (frameCounter_ + 1) % framesPerCycle;
}

void StrobeLight::process(ImageF32x4_RGBA& image, const StrobeLightSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
