Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));
    // Silhouette Alpha: dst shows only where src.alpha == 0
    float3 result = dst.rgb * (1.0 - step(0.001, src.a));
    float outAlpha = dst.a * (1.0 - step(0.001, src.a));
    ResultTex[DTid.xy] = float4(result, outAlpha);
}
