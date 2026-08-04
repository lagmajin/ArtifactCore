Texture2D<float4> ForegroundTexture : register(t0);
Texture2D<float4> CleanPlateTexture : register(t1);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float ScreenCorrection;
    float CoreMatteClip;
    float EdgeMatteSoftness;
    float DespillStrength;
    float GarbageMatteGamma;
    float DetailRecovery;
    uint ErodePixels;
    uint DilatePixels;
};

float safeFinite(float value, float fallback)
{
    return isfinite(value) ? value : fallback;
}

float smoothEdge(float value, float softness)
{
    float width = max(safeFinite(softness, 0.2f), 1.0e-5f);
    return smoothstep(0.0f, width, value);
}

float estimateMatte(uint2 pixel, uint2 dimensions)
{
    const float4 foreground = ForegroundTexture[pixel];
    const float4 cleanPlate = CleanPlateTexture[pixel];
    const float correction = max(safeFinite(ScreenCorrection, 1.0f), 0.0f);
    const float clip = saturate(safeFinite(CoreMatteClip, 0.5f));
    const float detail = saturate(safeFinite(DetailRecovery, 0.3f));
    const float gamma = max(safeFinite(GarbageMatteGamma, 1.0f), 1.0e-5f);
    const float3 corrected = max(foreground.rgb * correction, 0.0f);
    const float distance = length(corrected - cleanPlate.rgb);
    const float rawMatte = 1.0f - exp(-distance / 0.25f);
    const float coreMatte = saturate(rawMatte - clip);
    const float edgeMatte = smoothEdge(rawMatte * (1.0f - coreMatte), EdgeMatteSoftness);
    return pow(saturate(coreMatte + edgeMatte * detail), gamma);
}

float filteredMatte(uint2 pixel, uint2 dimensions)
{
    const uint erodeRadius = min(ErodePixels, 8u);
    const uint dilateRadius = min(DilatePixels, 8u);
    float value = estimateMatte(pixel, dimensions);
    for (int dy = -8; dy <= 8; ++dy) {
        for (int dx = -8; dx <= 8; ++dx) {
            const int distance = max(abs(dx), abs(dy));
            const int clampedX = clamp(int(pixel.x) + dx, 0, int(dimensions.x) - 1);
            const int clampedY = clamp(int(pixel.y) + dy, 0, int(dimensions.y) - 1);
            const float sample = estimateMatte(uint2(clampedX, clampedY), dimensions);
            if (distance <= int(erodeRadius)) value = min(value, sample);
            if (distance <= int(dilateRadius)) value = max(value, sample);
        }
    }
    return value;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    ForegroundTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    const uint2 pixel = dispatchId.xy;
    const float4 foreground = ForegroundTexture[pixel];
    const float4 cleanPlate = CleanPlateTexture[pixel];
    const float correction = max(safeFinite(ScreenCorrection, 1.0f), 0.0f);
    const float despill = saturate(safeFinite(DespillStrength, 0.5f));

    const float3 corrected = max(foreground.rgb * correction, 0.0f);
    const float matte = filteredMatte(pixel, uint2(width, height));
    const float alpha = saturate(matte * foreground.a);

    const float3 despilled = max(corrected - cleanPlate.rgb * despill, 0.0f);
    OutputTexture[pixel] = float4(despilled * alpha, alpha);
}
