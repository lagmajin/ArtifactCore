Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);

float3 ColorDodge(float3 base, float3 blend)
{
    float3 result;
    result.r = (blend.r < 1.0) ? saturate(base.r / (1.0 - blend.r)) : 1.0;
    result.g = (blend.g < 1.0) ? saturate(base.g / (1.0 - blend.g)) : 1.0;
    result.b = (blend.b < 1.0) ? saturate(base.b / (1.0 - blend.b)) : 1.0;
    return result;
}

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    float3 blended = ColorDodge(dst.rgb, src.rgb);
    float outAlpha = max(dst.a, src.a);

    ResultTex[DTid.xy] = float4(blended, outAlpha);
}
