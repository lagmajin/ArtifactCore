Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    float3 blended = dst.rgb - src.rgb;
    float outAlpha = src.a + dst.a * (1.0 - src.a);
    float3 outColor = saturate((src.a * blended + dst.rgb * dst.a * (1.0 - src.a)) / max(outAlpha, 1e-5));

    ResultTex[DTid.xy] = float4(outColor, outAlpha);
}
