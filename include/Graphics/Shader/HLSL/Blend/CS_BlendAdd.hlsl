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

    // 加算合成: Porter-Duff over + src.a で重み付けした加算
    float3 blended = dst.rgb + src.rgb;
    float outAlpha = src.a + dst.a * (1.0 - src.a);
    float3 outColor = saturate((src.a * blended + dst.rgb * dst.a * (1.0 - src.a)) / max(outAlpha, 1e-5));

    ResultTex[DTid.xy] = float4(outColor, outAlpha);

}
