Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    int Standard;   // 0 = NTSC, 1 = PAL
    float ReduceSaturation;
    float2 _pad;
};

static const float3x3 RGB2YUV = float3x3(
     0.299, -0.147,  0.615,
     0.587, -0.289, -0.515,
     0.114,  0.436, -0.100
);

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float4 color = InputTexture[ID.xy];
    float y = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    float u = dot(color.rgb, float3(-0.147f, -0.289f, 0.436f));
    float v = dot(color.rgb, float3(0.615f, -0.515f, -0.100f));

    float yLo = Standard == 0 ? 16.0f / 255.0f : 16.0f / 255.0f;
    float yHi = Standard == 0 ? 235.0f / 255.0f : 235.0f / 255.0f;
    float uvLimit = Standard == 0 ? 0.5f : 0.5f;

    y = clamp(y, yLo, yHi);
    u = clamp(u, -uvLimit, uvLimit) * (1.0f - ReduceSaturation);
    v = clamp(v, -uvLimit, uvLimit) * (1.0f - ReduceSaturation);

    float3x3 YUV2RGB = float3x3(1, 1, 1, 0, -0.394, 2.032, 1.140, -0.581, 0);
    float3 rgb = mul(YUV2RGB, float3(y, u, v));
    OutputTexture[ID.xy] = float4(rgb, color.a);
}
