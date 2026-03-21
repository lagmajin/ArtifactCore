#include "BlendCommon.hlsli"

Texture2D<float4> SrcTex  : register(t0);
Texture2D<float4> DstTex  : register(t1);
RWTexture2D<float4> ResultTex : register(u0);

[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(src.rgb);

    float3 outColor = HslToRgb(float3(blendHsl.x, baseHsl.y, baseHsl.z));
    float outAlpha = max(dst.a, src.a);

    ResultTex[DTid.xy] = float4(saturate(outColor), outAlpha);
}
