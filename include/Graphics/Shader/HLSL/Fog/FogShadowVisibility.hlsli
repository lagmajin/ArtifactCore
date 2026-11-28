#ifndef ARTIFACT_FOG_SHADOW_VISIBILITY_HLSLI
#define ARTIFACT_FOG_SHADOW_VISIBILITY_HLSLI

// Forward-Z light depth. clipToTexture contains UV scales, depth scale and
// depth bias. Samples outside the light map are treated as unoccluded.
bool FogLightOccluded(Texture2D<float> depthTexture, float4 lightClip,
                      float4 clipToTexture, bool occludeOutside = false)
{
    if (lightClip.w <= 0)
        return occludeOutside;
    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = ndc.xy * clipToTexture.xy + 0.5;
    float depth = mad(ndc.z, clipToTexture.z, clipToTexture.w);
    if (!all(uv >= 0) || !all(uv < 1) || !(depth >= 0 && depth <= 1))
        return occludeOutside;
    uint width, height;
    depthTexture.GetDimensions(width, height);
    uint2 texel = min(uint2(uv * float2(width, height)), uint2(width - 1, height - 1));
    return depth > depthTexture.Load(int3(texel, 0));
}

#endif
