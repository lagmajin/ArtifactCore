Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);

float3 OverlayBlend(float3 base, float3 blend)
{
    float3 result;
    result = (base < 0.5) ? (2.0 * base * blend) : (1.0 - 2.0 * (1.0 - base) * (1.0 - blend));
    return result;
}

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    float3 blended = OverlayBlend(dst.rgb, src.rgb);
    float outAlpha = max(dst.a, src.a);

    ResultTex[DTid.xy] = float4(blended, outAlpha);
}