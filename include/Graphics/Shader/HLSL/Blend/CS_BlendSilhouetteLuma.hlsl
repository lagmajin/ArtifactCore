Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));
    // Silhouette Luma: dst shows only where luminance(src) < threshold
    float3 result = dst.rgb * (1.0 - step(0.001, dot(src.rgb, float3(0.299, 0.587, 0.114))));
    float outAlpha = dst.a * (1.0 - step(0.001, dot(src.rgb, float3(0.299, 0.587, 0.114))));
    ResultTex[DTid.xy] = float4(result, outAlpha);
}
