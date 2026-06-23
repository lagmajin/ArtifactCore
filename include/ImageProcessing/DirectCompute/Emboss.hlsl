Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float Intensity;
    float Angle;
    float2 _pad;
};

static float3x3 K = float3x3(-1, 0, 1, -2, 0, 2, -1, 0, 1);
static float3x3 T = float3x3( 1, 2, 1,  0, 0, 0, -1,-2,-1);

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float4 center = InputTexture[ID.xy];
    float lumaCenter = dot(center.rgb, float3(0.299f, 0.587f, 0.114f));
    float gx = 0, gy = 0;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            uint2 coord = clamp(ID.xy + uint2(dx, dy), uint2(0, 0), uint2(width - 1, height - 1));
            float luma = dot(InputTexture[coord].rgb, float3(0.299f, 0.587f, 0.114f));
            gx += luma * K[dy + 1][dx + 1];
            gy += luma * T[dy + 1][dx + 1];
        }
    }
    float edge = gx * cos(Angle) + gy * sin(Angle);
    float3 result = center.rgb + edge * Intensity;
    OutputTexture[ID.xy] = float4(result, center.a);
}
