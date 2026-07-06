module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

module Render.VolumeRenderer;

namespace ArtifactCore::RayTrace {

namespace {

inline float clamp01(float v) noexcept {
    return std::clamp(v, 0.0f, 1.0f);
}

inline Color lerpColor(const Color& a, const Color& b, float t) noexcept {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

inline Vec3 safeNormalize(const Vec3& v) noexcept {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-12f) return {0.0f, 0.0f, 0.0f};
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen, v.z * invLen};
}

inline float dotColor(const Color& a, const Color& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

}

float VolumeScalarField::sampleTrilinear(const Vec3& voxelCoord) const noexcept {
    if (empty()) return 0.0f;

    const int w = resolution.width;
    const int h = resolution.height;
    const int d = resolution.depth;

    const float fx = std::clamp(voxelCoord.x, 0.0f, static_cast<float>(w - 1));
    const float fy = std::clamp(voxelCoord.y, 0.0f, static_cast<float>(h - 1));
    const float fz = std::clamp(voxelCoord.z, 0.0f, static_cast<float>(d - 1));

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int z0 = static_cast<int>(std::floor(fz));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const int z1 = std::min(z0 + 1, d - 1);

    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float tz = fz - static_cast<float>(z0);

    const float c000 = at(x0, y0, z0);
    const float c100 = at(x1, y0, z0);
    const float c010 = at(x0, y1, z0);
    const float c110 = at(x1, y1, z0);
    const float c001 = at(x0, y0, z1);
    const float c101 = at(x1, y0, z1);
    const float c011 = at(x0, y1, z1);
    const float c111 = at(x1, y1, z1);

    const float c00 = c000 + (c100 - c000) * tx;
    const float c10 = c010 + (c110 - c010) * tx;
    const float c01 = c001 + (c101 - c001) * tx;
    const float c11 = c011 + (c111 - c011) * tx;
    const float c0 = c00 + (c10 - c00) * ty;
    const float c1 = c01 + (c11 - c01) * ty;
    return c0 + (c1 - c0) * tz;
}

TransferFunctionSample TransferFunction::evaluate(float density, float temperature) const noexcept {
    const float d = density * densityScale;
    const float t = temperature * temperatureScale;

    if (customCallback) {
        return customCallback(d, t);
    }

    TransferFunctionSample sample{};
    sample.densityValue = d;

    switch (kind) {
    case TransferFunctionKind::Fire: {
        const float flameOpacity = std::clamp(d * 8.0f, 0.0f, 1.0f);
        const float heatFactor = clamp01(t * 0.5f + d * 0.5f);
        sample.emission = {
            clamp01(heatFactor * 2.0f),
            clamp01(heatFactor * 0.8f),
            clamp01(heatFactor * 0.15f)
        };
        sample.absorption = flameOpacity * 20.0f;
        break;
    }
    case TransferFunctionKind::Smoke: {
        const float opacity = clamp01(d * 10.0f);
        sample.emission = {opacity * 0.3f, opacity * 0.3f, opacity * 0.3f};
        sample.absorption = d * 30.0f;
        break;
    }
    case TransferFunctionKind::Steam: {
        const float opacity = clamp01(d * 3.0f);
        sample.emission = {opacity * 0.7f, opacity * 0.7f, opacity * 0.75f};
        sample.absorption = d * 5.0f;
        break;
    }
    case TransferFunctionKind::Explosion: {
        const float fire = clamp01(t * 0.7f);
        const float smoke = clamp01(d * 12.0f);
        sample.emission = {
            fire * 2.0f + smoke * 0.2f,
            fire * 0.6f + smoke * 0.15f,
            fire * 0.05f + smoke * 0.05f
        };
        sample.absorption = (fire + smoke) * 25.0f;
        break;
    }
    case TransferFunctionKind::Custom:
    default: {
        sample.emission = {d * 0.5f, d * 0.5f, d * 0.5f};
        sample.absorption = d * 10.0f;
        break;
    }
    }
    return sample;
}

TransferFunction TransferFunction::makeFire() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Fire;
    return tf;
}

TransferFunction TransferFunction::makeSmoke() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Smoke;
    return tf;
}

TransferFunction TransferFunction::makeSteam() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Steam;
    return tf;
}

TransferFunction TransferFunction::makeExplosion() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Explosion;
    return tf;
}

void CPUVolumeRenderer::setVolumeData(const VolumeAABB& aabb, const VolumeFieldSet& fieldSet) {
    bounds = aabb;
    fields = fieldSet;
}

void CPUVolumeRenderer::setTransferFunction(const TransferFunction& tf) {
    transferFunction_ = tf;
}

void CPUVolumeRenderer::addLight(const VolumetricLight& light) {
    lights.push_back(light);
}

void CPUVolumeRenderer::clearLights() {
    lights.clear();
}

Color CPUVolumeRenderer::evaluateVolumetricLights(const Vec3& worldPos, const Vec3& sampleDir) const noexcept {
    Color accumulated{0.0f, 0.0f, 0.0f};

    for (const auto& light : lights) {
        if (!light.enabled) continue;

        const Vec3 toLight{
            light.position.x - worldPos.x,
            light.position.y - worldPos.y,
            light.position.z - worldPos.z
        };
        const float distToLight = std::sqrt(toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z);

        if (distToLight > light.range) continue;

        const float rangeFactor = std::pow(std::max(0.0f, 1.0f - distToLight / light.range), light.falloffExponent);
        if (rangeFactor < 0.001f) continue;

        float directionalFactor = 1.0f;
        if (light.type == VolumeLightType::Spot) {
            const Vec3 lightForward = safeNormalize(light.direction);
            const Vec3 dirToLight = safeNormalize(toLight);
            const float cosAngle = -(lightForward.x * dirToLight.x + lightForward.y * dirToLight.y + lightForward.z * dirToLight.z);

            const float innerRad = light.coneInnerAngle * 3.14159265f / 180.0f;
            const float outerRad = light.coneOuterAngle * 3.14159265f / 180.0f;
            const float cosInner = std::cos(innerRad * 0.5f);
            const float cosOuter = std::cos(outerRad * 0.5f);

            if (cosAngle < cosOuter) continue;

            if (cosAngle < cosInner) {
                const float t = (cosAngle - cosOuter) / (cosInner - cosOuter);
                directionalFactor = std::pow(t, light.coneSoftness);
            }
        }

        const float viewScatter = std::max(0.0f, sampleDir.x * sampleDir.x + sampleDir.y * sampleDir.y + sampleDir.z * sampleDir.z) * 0.1f + 0.9f;
        const float intensity = light.intensity * rangeFactor * directionalFactor * viewScatter;

        accumulated.x += light.color.x * intensity;
        accumulated.y += light.color.y * intensity;
        accumulated.z += light.color.z * intensity;
    }

    return accumulated;
}

bool CPUVolumeRenderer::intersectAABB(const Ray& ray, float& tNear, float& tFar) const noexcept {
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();

    for (int i = 0; i < 3; ++i) {
        const float origin = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
        const float direction = (i == 0) ? ray.dir.x : (i == 1) ? ray.dir.y : ray.dir.z;
        const float bmin = (i == 0) ? bounds.min.x : (i == 1) ? bounds.min.y : bounds.min.z;
        const float bmax = (i == 0) ? bounds.max.x : (i == 1) ? bounds.max.y : bounds.max.z;

        if (std::abs(direction) < 1e-8f) {
            if (origin < bmin || origin > bmax) return false;
            continue;
        }

        const float invDir = 1.0f / direction;
        float t0 = (bmin - origin) * invDir;
        float t1 = (bmax - origin) * invDir;
        if (t0 > t1) std::swap(t0, t1);

        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmin > tmax) return false;
    }

    tNear = std::max(0.0f, tmin);
    tFar = tmax;
    return tNear <= tFar;
}

Vec3 CPUVolumeRenderer::worldToVoxel(const Vec3& worldPos) const noexcept {
    const auto e = bounds.extent();
    return {
        (worldPos.x - bounds.min.x) / (e.x > 0.0f ? e.x : 1.0f) * static_cast<float>(fields.density.resolution.width),
        (worldPos.y - bounds.min.y) / (e.y > 0.0f ? e.y : 1.0f) * static_cast<float>(fields.density.resolution.height),
        (worldPos.z - bounds.min.z) / (e.z > 0.0f ? e.z : 1.0f) * static_cast<float>(fields.density.resolution.depth),
    };
}

float CPUVolumeRenderer::sampleDensity(const Vec3& voxelPos) const noexcept {
    if (fields.density.empty()) return 0.0f;
    return fields.density.sampleTrilinear(voxelPos);
}

float CPUVolumeRenderer::phaseHenyeyGreenstein(float cosTheta, float g) const noexcept {
    const float gSq = g * g;
    const float denom = 1.0f + gSq - 2.0f * g * cosTheta;
    return (1.0f - gSq) / (4.0f * 3.14159265f * std::pow(std::max(denom, 0.001f), 1.5f));
}

float CPUVolumeRenderer::multipleScatteringBoost(float density, float depth) const noexcept {
    if (settings.multipleScatteringStrength <= 0.0f) return 1.0f;
    const float di = std::clamp(density * settings.multipleScatteringSpread, 0.0f, 1.0f);
    const float dp = std::clamp(depth * settings.multipleScatteringSpread * 0.1f, 0.0f, 1.0f);
    float boost = 0.0f;
    float weightSum = 0.0f;
    float freq = 1.0f;
    for (int i = 0; i < settings.multipleScatteringOctaves; ++i) {
        boost += di * freq;
        weightSum += freq;
        freq *= 0.5f;
    }
    boost = boost / std::max(weightSum, 1e-10f);
    return 1.0f + boost * settings.multipleScatteringStrength * (1.0f + dp);
}

Color CPUVolumeRenderer::sampleLight(const Vec3& voxelPos) const noexcept {
    if (settings.shadowIntensity <= 0.0f || fields.density.empty()) return {1.0f, 1.0f, 1.0f};

    const Vec3 lightDir = safeNormalize(settings.lightDirection);
    float t = 0.0f;
    float accumulatedDensity = 0.0f;

    for (int i = 0; i < settings.shadowMaxSteps; ++i) {
        t += settings.shadowStepSize;
        const Vec3 samplePos{
            voxelPos.x + lightDir.x * t,
            voxelPos.y + lightDir.y * t,
            voxelPos.z + lightDir.z * t
        };
        accumulatedDensity += sampleDensity(samplePos) * settings.shadowDensityScale;

        if (accumulatedDensity > 8.0f) break;
    }

    const float transmittance = std::exp(-accumulatedDensity);
    return {transmittance, transmittance, transmittance};
}

Color CPUVolumeRenderer::raymarch(const Ray& ray) const noexcept {
    float tNear = 0.0f;
    float tFar = 0.0f;

    if (!intersectAABB(ray, tNear, tFar)) {
        return settings.backgroundColor;
    }

    if (fields.density.empty()) {
        return settings.backgroundColor;
    }

    const Vec3 lightDir = safeNormalize(settings.lightDirection);
    float t = tNear;
    float transmittance = 1.0f;
    Color accumulated{0.0f, 0.0f, 0.0f};

    for (int i = 0; i < settings.maxSteps && t < tFar; ++i) {
        if (transmittance < 0.01f) break;

        const Vec3 worldPos = ray.at(t);
        const Vec3 voxelCoord = worldToVoxel(worldPos);

        const float density = sampleDensity(voxelCoord);
        if (density <= 0.0f) {
            t += settings.stepSize;
            continue;
        }

        float temperature = 0.0f;
        if (settings.useTemperatureEmission && !fields.temperature.empty()) {
            temperature = fields.temperature.sampleTrilinear(voxelCoord);
        }

        const auto tfSample = transferFunction_.evaluate(density * settings.densityScale, temperature);
        const float absorption = tfSample.absorption * settings.stepSize;

        Color lighting{1.0f, 1.0f, 1.0f};
        if (settings.lightIntensity > 0.0f) {
            const auto shadow = sampleLight(voxelCoord);
            const float NdotL = std::max(0.0f, -settings.lightDirection.y);
            const float henyey = phaseHenyeyGreenstein(NdotL, settings.phaseAnisotropy);
            lighting = {
                shadow.x * (0.1f + henyey * 0.9f) * settings.lightIntensity,
                shadow.y * (0.1f + henyey * 0.9f) * settings.lightIntensity,
                shadow.z * (0.1f + henyey * 0.9f) * settings.lightIntensity
            };
        }

        const auto volumetricLightContrib = evaluateVolumetricLights(worldPos, ray.dir);
        const float scatteringBoost = multipleScatteringBoost(density * settings.densityScale, t - tNear);

        const float alpha = 1.0f - std::exp(-absorption);
        accumulated.x += transmittance * alpha * ((tfSample.emission.x * lighting.x + volumetricLightContrib.x) * scatteringBoost);
        accumulated.y += transmittance * alpha * ((tfSample.emission.y * lighting.y + volumetricLightContrib.y) * scatteringBoost);
        accumulated.z += transmittance * alpha * ((tfSample.emission.z * lighting.z + volumetricLightContrib.z) * scatteringBoost);
        transmittance *= (1.0f - alpha);

        t += settings.stepSize;
    }

    accumulated.x += settings.backgroundColor.x * transmittance;
    accumulated.y += settings.backgroundColor.y * transmittance;
    accumulated.z += settings.backgroundColor.z * transmittance;

    return accumulated;
}

ImageBuffer CPUVolumeRenderer::render(int width, int height) const {
    ImageBuffer buffer(width, height);
    camera.setViewport(static_cast<float>(width), static_cast<float>(height));

    const int dofSamples = camera.aperture > 0.0f ? 4 : 1;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color pixelColor{0.0f, 0.0f, 0.0f};

            for (int s = 0; s < dofSamples; ++s) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);

                if (dofSamples == 1) {
                    pixelColor = raymarch(camera.getRay(u, v));
                } else {
                    const float goldenAngle = 2.39996323f;
                    const float theta = static_cast<float>(s) * goldenAngle;
                    const float radius = std::sqrt(static_cast<float>(s) + 0.5f) / std::max(std::sqrt(static_cast<float>(dofSamples)), 1.0f);
                    float lensU = std::cos(theta) * radius;
                    float lensV = std::sin(theta) * radius;
                    pixelColor.x += raymarch(camera.getRayDOF(u, v, lensU, lensV)).x;
                    pixelColor.y += raymarch(camera.getRayDOF(u, v, lensU, lensV)).y;
                    pixelColor.z += raymarch(camera.getRayDOF(u, v, lensU, lensV)).z;
                }
            }

            if (dofSamples > 1) {
                const float invSamples = 1.0f / static_cast<float>(dofSamples);
                pixelColor.x *= invSamples;
                pixelColor.y *= invSamples;
                pixelColor.z *= invSamples;
            }

            const auto offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3ull + static_cast<std::size_t>(x) * 3ull;
            buffer.pixels[offset + 0] = static_cast<std::uint8_t>(std::clamp(pixelColor.x * 255.999f, 0.0f, 255.0f));
            buffer.pixels[offset + 1] = static_cast<std::uint8_t>(std::clamp(pixelColor.y * 255.999f, 0.0f, 255.0f));
            buffer.pixels[offset + 2] = static_cast<std::uint8_t>(std::clamp(pixelColor.z * 255.999f, 0.0f, 255.0f));
        }
    }

    return buffer;
}

float normalizeStepToVoxel(float stepSize, const VolumeAABB& bounds, const VolumeResolution& resolution) noexcept {
    if (!resolution.valid()) return stepSize;
    const auto vs = bounds.voxelSize(resolution);
    const float voxelDiag = std::sqrt(vs.x * vs.x + vs.y * vs.y + vs.z * vs.z);
    return std::max(stepSize, voxelDiag * 0.25f);
}

} // namespace ArtifactCore::RayTrace