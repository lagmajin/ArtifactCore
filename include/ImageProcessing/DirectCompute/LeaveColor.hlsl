Texture2D<float4> InputTexture  : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    float4 KeyColor;
    float Tolerance;
    float Softness;
    float DesaturateAmount;
    float _pad;
};

float colorDist(float3 a, float3 b) {
    return length(a - b) * 0.57735f;
}

float luma(float3 c) {
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);
    if (ID.x >= width || ID.y >= height) return;

    float4 src = InputTexture[ID.xy];
    float dist = colorDist(src.rgb, KeyColor.rgb);
    float mix = saturate((dist - Tolerance) / max(Softness, 0.001f));
    float gray = luma(src.rgb);
    float3 desat = float3(gray, gray, gray);
    float3 result = lerp(src.rgb, desat, mix * DesaturateAmount);
    OutputTexture[ID.xy] = float4(result, src.a);
}
