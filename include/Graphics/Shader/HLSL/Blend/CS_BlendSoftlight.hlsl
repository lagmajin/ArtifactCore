Texture2D<float4> SrcTex  : register(t0); // 前景
Texture2D<float4> DstTex  : register(t1); // 背景
RWTexture2D<float4> ResultTex : register(u0);
float SoftLightChannel(float base, float blend)
{
    return (blend < 0.5)
        ? base - (1.0 - 2.0 * blend) * base * (1.0 - base)
        : base + (2.0 * blend - 1.0) * (sqrt(base) - base);
}

float3 SoftLightBlend(float3 base, float3 blend)
{
    return float3(
        SoftLightChannel(base.r, blend.r),
        SoftLightChannel(base.g, blend.g),
        SoftLightChannel(base.b, blend.b)
    );
}
[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    float3 blended = SoftLightBlend(dst.rgb, src.rgb);
    float outAlpha = max(dst.a, src.a); // アルファ合成は用途に応じて調整

    ResultTex[DTid.xy] = float4(blended, outAlpha);
}