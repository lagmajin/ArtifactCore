#ifndef ARTIFACT_LOCAL_FOG_INTEGRATION_HLSLI
#define ARTIFACT_LOCAL_FOG_INTEGRATION_HLSLI

#include "FogShadowVisibility.hlsli"

// Evaluate a bounded world-space medium clipped to the visible ray segment.
float4 IntegrateLocalFog(float4 source, float3 ray, float distance,
                         float3 cameraPosition, float3 volumeLow, float3 volumeHigh,
                         float extinctionAtFullDensity, float3 fogTint, float windSpeed,
                         float timeSeconds, uint mode, uint sampleCount, float4 beamShape,
                         float3 beamTint, float4x4 worldToLightClip, float4 lightClipToTexture,
                         Texture2D<float> lightDepthTexture)
{
    if (extinctionAtFullDensity <= 0)
        return source;
    const float3 camera = cameraPosition;
    const float3 low = volumeLow;
    const float3 high = volumeHigh;
    const float3 fogColor = fogTint;
    float start = 0, end = distance;
    [unroll] for (uint axis = 0; axis < 3; ++axis)
    {
        if (abs(ray[axis]) < 1e-12)
        {
            if (camera[axis] < low[axis] || camera[axis] > high[axis])
                end = 0;
        }
        else
        {
            float a = (low[axis] - camera[axis]) / ray[axis];
            float b = (high[axis] - camera[axis]) / ray[axis];
            start = max(start, min(a, b));
            end = min(end, max(a, b));
        }
    }
    if (end <= start)
    {
        return source;
    }
    if (mode == 0)
    {
        float visibility = exp(-extinctionAtFullDensity * (end - start));
        return float4(lerp(fogColor, source.rgb, visibility), source.a);
    }

    const uint steps = clamp(sampleCount, 1u, 512u);
    float ds = (end - start) / steps;
    float transmittance = 1, light = 0;
    [loop] for (uint i = 0; i < steps; ++i)
    {
        float3 p = camera + ray * (start + (i + 0.5) * ds);
        float variation = 0.5 + 0.5 * sin(0.45 * (p.x - windSpeed * timeSeconds) + 0.19 * p.z);
        // Turning on scene light must not add density variation to still fog.
        float densityScale = mode == 4 && windSpeed == 0 ? 1 : 0.25 + 0.75 * variation;
        float stepT = exp(-extinctionAtFullDensity * densityScale * ds);
        if (mode == 4)
        {
            // Scene lighting uses the actual light frustum and its occluders.
            if (!FogLightOccluded(lightDepthTexture,
                    mul(worldToLightClip, float4(p, 1)), lightClipToTexture, true))
                light += transmittance * (1 - stepT);
        }
        else if (mode >= 2)
        {
            float2 beamOffset = float2((p.x - beamShape.x - beamShape.y * (p.z - 12)) / 2,
                                       p.y - beamShape.z - beamShape.w * (p.z - 12));
            float profile = max(0, 1 - dot(beamOffset, beamOffset));
            // Project along the beam to the orthographic light-depth map.
            bool shadowed = false;
            if (mode == 3 && profile > 0)
            {
                shadowed = FogLightOccluded(lightDepthTexture,
                    mul(worldToLightClip, float4(p, 1)), lightClipToTexture);
            }
            if (!shadowed)
                light += transmittance * (1 - stepT) * profile * profile;
        }
        transmittance *= stepT;
    }
    return float4(lerp(fogColor, source.rgb, transmittance)
                                 + beamTint * light, source.a);
}

#endif
