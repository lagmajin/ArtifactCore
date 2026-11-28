#ifndef ARTIFACT_HEIGHT_FOG_MATH_HLSLI
#define ARTIFACT_HEIGHT_FOG_MATH_HLSLI

// Endpoints are heights relative to ground. Below ground the density is constant.
float IntegratedDensity(float h0, float h1, float distance, float heightFalloff)
{
    float low = min(h0, h1);
    float high = max(h0, h1);
    if (high <= 0.0)
        return distance;

    float below = low < 0.0 ? distance * (-low / (high - low)) : 0.0;
    float startHeight = max(low, 0.0);
    float k = heightFalloff * (high - startHeight);
    // Limit of (1 - exp(-k)) / k near zero avoids cancellation.
    float mean = k < 0.01 ? 1.0 - k * (0.5 - k / 6.0) : (1.0 - exp(-k)) / k;
    return below + (distance - below) * exp(-heightFalloff * startHeight) * mean;
}

float4 ApplyHeightFog(float4 source, float3 fogColor, float opticalDepth, bool premultiplied)
{
    float transmittance = exp(-opticalDepth);
    float3 fog = premultiplied ? fogColor * source.a : fogColor;
    return float4(lerp(fog, source.rgb, transmittance), source.a);
}

#endif
