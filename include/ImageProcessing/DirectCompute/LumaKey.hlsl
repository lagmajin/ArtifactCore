Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float LowThreshold;
    float HighThreshold;
    float Softness;
};

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float4 color = InputTexture[ID.xy];
    float luma = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    float softInv = Softness > 0.0f ? 1.0f / Softness : 1.0f / 0.001f;
    float alpha = 1.0f;
    if (luma < LowThreshold)
        alpha = saturate((luma - (LowThreshold - Softness)) * softInv);
    else if (luma > HighThreshold)
        alpha = saturate(((HighThreshold + Softness) - luma) * softInv);
    OutputTexture[ID.xy] = float4(color.rgb, color.a * alpha);
}
