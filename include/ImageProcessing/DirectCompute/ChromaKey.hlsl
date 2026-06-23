Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float4 KeyColor;
    float Tolerance;
    float Softness;
    float2 _pad;
};

float colorDist(float3 a, float3 b) {
    return length(a - b) * 0.57735f;
}

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float4 src = InputTexture[ID.xy];
    float dist = colorDist(src.rgb, KeyColor.rgb);
    float alpha = saturate((dist - Tolerance) / max(Softness, 0.001f));
    OutputTexture[ID.xy] = float4(src.rgb, src.a * (1.0f - alpha));
}
