Texture2D<float4> SrcTex  : register(t0); // 前景
Texture2D<float4> DstTex  : register(t1); // 背景
RWTexture2D<float4> ResultTex : register(u0);
float3 ScreenBlend(float3 base, float3 blend)
{
    // スクリーンブレンドの計算式: 1 - (1 - base) * (1 - blend)
    // 通常、色値は0.0から1.0の範囲と仮定します。
    return 1.0 - (1.0 - base) * (1.0 - blend);
}
[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    float3 blended = ScreenBlend(dst.rgb, src.rgb);
    float outAlpha = max(dst.a, src.a); // アルファの扱いは用途次第で調整

    ResultTex[DTid.xy] = float4(blended, outAlpha);
}