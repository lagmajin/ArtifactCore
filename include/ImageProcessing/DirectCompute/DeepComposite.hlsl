struct DeepSample
{
    float depth;
    float depthBack;
    float4 color;
    float alpha;
    float coverage;
    uint holdout;
    uint _samplePadding;
};

StructuredBuffer<DeepSample> FrontSamples : register(t0);
StructuredBuffer<uint> FrontOffsets : register(t1);
StructuredBuffer<uint> FrontCounts : register(t2);
StructuredBuffer<DeepSample> BackSamples : register(t3);
StructuredBuffer<uint> BackOffsets : register(t4);
StructuredBuffer<uint> BackCounts : register(t5);
RWStructuredBuffer<float4> OutputPixels : register(u0);

cbuffer DeepCompositeParams : register(b0)
{
    uint Width;
    uint Height;
    uint MaxSamplesPerPixel;
    uint _Padding;
};

float sampleAlpha(DeepSample sample)
{
    return saturate(sample.alpha * sample.coverage);
}

[numthreads(8, 8, 1)]
void CSDeepCompositeOver(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= Width || dispatchId.y >= Height) return;
    const uint pixelIndex = dispatchId.y * Width + dispatchId.x;
    uint frontIndex = 0;
    uint backIndex = 0;
    const uint frontCount = MaxSamplesPerPixel == 0
        ? FrontCounts[pixelIndex]
        : min(FrontCounts[pixelIndex], MaxSamplesPerPixel);
    const uint backCount = MaxSamplesPerPixel == 0
        ? BackCounts[pixelIndex]
        : min(BackCounts[pixelIndex], MaxSamplesPerPixel);
    const uint frontOffset = FrontOffsets[pixelIndex];
    const uint backOffset = BackOffsets[pixelIndex];
    float3 color = 0.0f;
    float transmittance = 1.0f;

    while ((frontIndex < frontCount || backIndex < backCount) && transmittance > 1.0e-4f)
    {
        bool useFront = backIndex >= backCount;
        if (frontIndex < frontCount && backIndex < backCount)
        {
            useFront = FrontSamples[frontOffset + frontIndex].depth <=
                       BackSamples[backOffset + backIndex].depth;
        }
        DeepSample sample = useFront
            ? FrontSamples[frontOffset + frontIndex++]
            : BackSamples[backOffset + backIndex++];
        const float alpha = sampleAlpha(sample);
        if (sample.holdout == 0)
            color += sample.color.rgb * alpha * transmittance;
        transmittance *= 1.0f - alpha;
    }
    OutputPixels[pixelIndex] = float4(color, 1.0f - transmittance);
}
