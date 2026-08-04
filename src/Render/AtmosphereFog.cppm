module;
#include <algorithm>
#include <cmath>
#include <cstdint>

module Render.AtmosphereFog;

import Core.Parallel;

namespace ArtifactCore::RayTrace {

namespace {

inline Vec3 safeNormalize(const Vec3& v) noexcept {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-12f) return {0.0f, 0.0f, 0.0f};
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen, v.z * invLen};
}

inline Color lerpColor(const Color& a, const Color& b, float t) noexcept {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

}

void AtmosphereFogRenderer::setSettings(const AtmosphereFogSettings& settings) {
    settings_ = settings;
    settings_.density = std::isfinite(settings_.density)
        ? std::max(0.0f, settings_.density) : 0.0f;
    settings_.heightFalloff = std::isfinite(settings_.heightFalloff)
        ? std::max(0.0f, settings_.heightFalloff) : 0.0f;
    settings_.absorption = std::isfinite(settings_.absorption)
        ? std::max(0.0f, settings_.absorption) : 0.0f;
    settings_.scattering = std::isfinite(settings_.scattering)
        ? std::max(0.0f, settings_.scattering) : 0.0f;
    settings_.sunIntensity = std::isfinite(settings_.sunIntensity)
        ? std::max(0.0f, settings_.sunIntensity) : 0.0f;
    settings_.samples = std::clamp(settings_.samples, 1, 4096);
    settings_.maxDistance = std::isfinite(settings_.maxDistance)
        ? std::max(0.0f, settings_.maxDistance) : 0.0f;
}

float AtmosphereFogRenderer::heightDensity(float height) const noexcept {
    return settings_.density * std::exp(-std::max(0.0f, height - settings_.groundLevel) * settings_.heightFalloff);
}

float AtmosphereFogRenderer::phaseRayleigh(float cosTheta) const noexcept {
    const float cosSq = cosTheta * cosTheta;
    return 0.75f * (1.0f + cosSq) * (3.0f / (16.0f * 3.14159265f));
}

Color AtmosphereFogRenderer::evaluateFog(const Ray& ray, float tMax, const Color& backgroundColor) const noexcept {
    if (!settings_.enabled) return backgroundColor;

    const float clampedTMax = std::min(tMax, settings_.maxDistance);
    if (clampedTMax <= 0.0f) return backgroundColor;

    const float dt = clampedTMax / static_cast<float>(settings_.samples);

    const float sunRad = settings_.sunAngle * 3.14159265f / 180.0f;
    const Vec3 sunDir{std::cos(sunRad), -std::sin(sunRad), 0.0f};

    float totalTransmittance = 0.0f;
    Color scattered{0.0f, 0.0f, 0.0f};

    for (int i = 0; i < settings_.samples; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) * dt;
        const Vec3 pos = ray.at(t);
        const float hDensity = heightDensity(pos.y);

        if (hDensity <= 0.0f) continue;

        const float cosTheta = -(sunDir.x * ray.dir.x + sunDir.y * ray.dir.y + sunDir.z * ray.dir.z);
        const float phase = phaseRayleigh(cosTheta);

        const float sampleDensity = hDensity * dt;
        const float sampleAbsorption = std::exp(-totalTransmittance - sampleDensity * settings_.absorption * 0.5f);

        scattered.x += sampleDensity * settings_.scattering * phase * settings_.fogColor.x * sampleAbsorption * settings_.sunIntensity;
        scattered.y += sampleDensity * settings_.scattering * phase * settings_.fogColor.y * sampleAbsorption * settings_.sunIntensity;
        scattered.z += sampleDensity * settings_.scattering * phase * settings_.fogColor.z * sampleAbsorption * settings_.sunIntensity;

        totalTransmittance += sampleDensity * settings_.absorption;
    }

    const float fogTransmittance = std::exp(-totalTransmittance);
    const float heightRatio = std::clamp(ray.dir.y * 0.5f + 0.5f, 0.0f, 1.0f);
    const Color fogBlend = lerpColor(settings_.horizonColor, settings_.fogColor, heightRatio);

    Color result;
    result.x = backgroundColor.x * fogTransmittance + scattered.x + fogBlend.x * (1.0f - fogTransmittance) * 0.5f;
    result.y = backgroundColor.y * fogTransmittance + scattered.y + fogBlend.y * (1.0f - fogTransmittance) * 0.5f;
    result.z = backgroundColor.z * fogTransmittance + scattered.z + fogBlend.z * (1.0f - fogTransmittance) * 0.5f;

    return result;
}

Color AtmosphereFogRenderer::applyToPixel(const Color& pixelColor, float depth, float nearPlane, float farPlane) const noexcept {
    if (!settings_.enabled || !std::isfinite(depth) ||
        !std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
        farPlane <= nearPlane || depth < nearPlane) return pixelColor;

    const float clampedDepth = std::clamp(depth, nearPlane, farPlane);
    const float maxDist = std::min(settings_.maxDistance, farPlane - nearPlane);
    const float tMax = (clampedDepth - nearPlane) / (farPlane - nearPlane) * maxDist;

    const float hDensity = settings_.density * settings_.heightFalloff;
    const float opticalDepth = hDensity * tMax;
    const float transmittance = std::exp(-opticalDepth * settings_.absorption);

    const float heightRatio = 0.5f;
    const Color fogBlend = lerpColor(settings_.horizonColor, settings_.fogColor, heightRatio);

    Color result;
    result.x = pixelColor.x * transmittance + fogBlend.x * (1.0f - transmittance);
    result.y = pixelColor.y * transmittance + fogBlend.y * (1.0f - transmittance);
    result.z = pixelColor.z * transmittance + fogBlend.z * (1.0f - transmittance);

    return result;
}

ImageBuffer AtmosphereFogRenderer::applyToImage(const ImageBuffer& depthBuffer, const ImageBuffer& colorBuffer,
                                                  float nearPlane, float farPlane) const noexcept {
    if (!settings_.enabled || !std::isfinite(nearPlane) ||
        !std::isfinite(farPlane) || farPlane <= nearPlane) return colorBuffer;

    ImageBuffer result = colorBuffer;
    const int w = std::min(depthBuffer.width, colorBuffer.width);
    const int h = std::min(depthBuffer.height, colorBuffer.height);

Parallel::For(0, h, w * h, [&](int y) {
        const std::uint8_t* depthRow = depthBuffer.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
        const std::uint8_t* colorRow = colorBuffer.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
        std::uint8_t* resultRow = result.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
        for (int x = 0; x < w; ++x) {
            const auto pixelOffset = static_cast<std::size_t>(x) * 3u;
            const float depth = static_cast<float>(depthRow[pixelOffset]) / 255.0f;

            Color pixel{
                static_cast<float>(colorRow[pixelOffset + 0]) / 255.0f,
                static_cast<float>(colorRow[pixelOffset + 1]) / 255.0f,
                static_cast<float>(colorRow[pixelOffset + 2]) / 255.0f,
            };

            const auto fogged = applyToPixel(pixel, depth * farPlane, nearPlane, farPlane);

            resultRow[pixelOffset + 0] = static_cast<std::uint8_t>(std::clamp(fogged.x * 255.999f, 0.0f, 255.0f));
            resultRow[pixelOffset + 1] = static_cast<std::uint8_t>(std::clamp(fogged.y * 255.999f, 0.0f, 255.0f));
            resultRow[pixelOffset + 2] = static_cast<std::uint8_t>(std::clamp(fogged.z * 255.999f, 0.0f, 255.0f));
        }
    });

    return result;
}

} // namespace ArtifactCore::RayTrace
