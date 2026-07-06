module;
#include <algorithm>
#include <cmath>
#include <cstdint>

export module Render.AtmosphereFog;

import Render.Vector3D;
import Render.Ray;
import Render.Camera;
import Render.ImageBuffer;
import Render.VolumeRenderer;

export namespace ArtifactCore::RayTrace {

struct AtmosphereFogSettings {
    float density = 0.1f;
    float heightFalloff = 0.5f;
    float groundLevel = 0.0f;
    Color fogColor{0.5f, 0.6f, 0.7f};
    Color horizonColor{0.67f, 0.53f, 0.36f};
    float sunAngle = 45.0f;
    float sunIntensity = 1.0f;
    float scattering = 0.3f;
    float absorption = 0.1f;
    int samples = 64;
    float maxDistance = 100.0f;
    bool enabled = true;
};

class AtmosphereFogRenderer {
public:
    AtmosphereFogRenderer() = default;

    void setSettings(const AtmosphereFogSettings& settings);

    [[nodiscard]] Color evaluateFog(const Ray& ray, float tMax, const Color& backgroundColor) const noexcept;
    [[nodiscard]] ImageBuffer applyToImage(const ImageBuffer& depthBuffer, const ImageBuffer& colorBuffer,
                                            float nearPlane, float farPlane) const noexcept;
    [[nodiscard]] Color applyToPixel(const Color& pixelColor, float depth, float nearPlane, float farPlane) const noexcept;

private:
    AtmosphereFogSettings settings_;

    [[nodiscard]] float heightDensity(float height) const noexcept;
    [[nodiscard]] float phaseRayleigh(float cosTheta) const noexcept;
};

} // namespace ArtifactCore::RayTrace