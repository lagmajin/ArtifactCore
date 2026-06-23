Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float Choke;
    int Radius;
    float2 _pad;
};

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float center = InputTexture[ID.xy].a;
    float minA = 1.0f, maxA = 0.0f;
    int r = max(1, Radius);
    [loop]
    for (int dy = -r; dy <= r; ++dy) {
        [loop]
        for (int dx = -r; dx <= r; ++dx) {
            uint2 coord = clamp(ID.xy + uint2(dx, dy), uint2(0, 0), uint2(width - 1, height - 1));
            float a = InputTexture[coord].a;
            minA = min(minA, a);
            maxA = max(maxA, a);
        }
    }
    float result;
    if (Choke >= 0.0f) {
        result = lerp(center, minA, Choke);
    } else {
        result = lerp(center, maxA, -Choke);
    }
    float4 src = InputTexture[ID.xy];
    OutputTexture[ID.xy] = float4(src.rgb, result);
}
