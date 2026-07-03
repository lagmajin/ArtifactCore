Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);
float3 VividLight(float3 base, float3 blend) {
    float3 r;
    r.r = (blend.r < 0.5) ? 1.0-(1.0-base.r)/max(2.0*blend.r,1e-5) : base.r/max(2.0*(1.0-blend.r),1e-5);
    r.g = (blend.g < 0.5) ? 1.0-(1.0-base.g)/max(2.0*blend.g,1e-5) : base.g/max(2.0*(1.0-blend.g),1e-5);
    r.b = (blend.b < 0.5) ? 1.0-(1.0-base.b)/max(2.0*blend.b,1e-5) : base.b/max(2.0*(1.0-blend.b),1e-5);
    return saturate(r);
}
[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));
    float3 blended = VividLight(dst.rgb, src.rgb);
    float outAlpha = src.a + dst.a * (1.0 - src.a);
    float3 outColor = (src.a * blended + dst.rgb * dst.a * (1.0 - src.a)) / max(outAlpha, 1e-5);
    ResultTex[DTid.xy] = float4(outColor, outAlpha);
}
