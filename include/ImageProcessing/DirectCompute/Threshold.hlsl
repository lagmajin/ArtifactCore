Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float Threshold;
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
    float t = (luma - Threshold + Softness * 0.5f) / max(Softness, 0.001f);
    t = saturate(t);
    OutputTexture[ID.xy] = float4(t, t, t, color.a);
}
