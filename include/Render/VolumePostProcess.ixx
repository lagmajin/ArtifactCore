module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

export module Render.VolumePostProcess;

import Render.Vector3D;
import Render.ImageBuffer;
import Render.VolumeRenderer;

export namespace ArtifactCore::RayTrace {

struct BloomSettings {
    float threshold = 0.7f;
    float intensity = 1.0f;
    float radius = 0.02f;
    int iterations = 4;
    bool enabled = true;
};

struct GlareSettings {
    float intensity = 0.5f;
    int streakCount = 6;
    float streakLength = 0.05f;
    float angleOffset = 0.0f;
    bool enabled = true;
};

struct DenoiseSettings {
    float spatialSigma = 1.0f;
    float rangeSigma = 0.1f;
    int filterRadius = 2;
    bool enabled = false;
};

struct VolumePostProcessSettings {
    BloomSettings bloom;
    GlareSettings glare;
    DenoiseSettings denoise;
    float exposure = 1.0f;
    float gamma = 2.2f;
};

class VolumePostProcessor {
public:
    void setSettings(const VolumePostProcessSettings& settings);
    [[nodiscard]] const VolumePostProcessSettings& settings() const noexcept { return settings_; }

    void process(ImageBuffer& image) const noexcept;

private:
    VolumePostProcessSettings settings_;

    void applyBloom(ImageBuffer& image) const noexcept;
    void applyGlare(ImageBuffer& image) const noexcept;
    void applyBilateralFilter(ImageBuffer& image) const noexcept;
    void applyExposureGamma(ImageBuffer& image) const noexcept;

    [[nodiscard]] float pixelLuminance(const ImageBuffer& image, int x, int y) const noexcept;
    [[nodiscard]] Color pixelColor(const ImageBuffer& image, int x, int y) const noexcept;
    void setPixel(ImageBuffer& image, int x, int y, const Color& color) const noexcept;
};

} // namespace ArtifactCore::RayTrace