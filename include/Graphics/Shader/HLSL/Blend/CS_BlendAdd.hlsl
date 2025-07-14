Texture2D<float4> SrcTex  : register(t0); // 前景
Texture2D<float4> DstTex  : register(t1); // 背景

// 出力テクスチャ（書き込み用）
RWTexture2D<float4> ResultTex : register(u0);

// スレッドグループサイズ（適宜調整）
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    // 加算合成（clampしないとHDRでぶっ飛ぶ）
    float3 outColor = saturate(dst.rgb + src.rgb); // RGBの加算
    float outAlpha = saturate(dst.a + src.a);      // アルファも加算（必要なら）

    ResultTex[DTid.xy] = float4(outColor, outAlpha);

}
