Texture2D<float4> SrcTex  : register(t0); // 前景テクスチャ（RGBA順想定）
Texture2D<float4> DstTex  : register(t1); // 背景テクスチャ（RGBA順想定）

// 出力テクスチャ：UAV（Unordered Access View） 1枚
RWTexture2D<float4> ResultTex : register(u0);

// スレッドグループサイズ
[numthreads(8,8,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // テクスチャサイズはCPU側で揃えておくこと
    float4 src = SrcTex.Load(int3(DTid.xy, 0));
    float4 dst = DstTex.Load(int3(DTid.xy, 0));

    // ノーマル合成（非プリマルチアルファ）
    float outAlpha = src.a + dst.a * (1.0 - src.a);

    // 0除算防止のためmaxでガード
    float3 outColor = (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / max(outAlpha, 1e-5);

    ResultTex[DTid.xy] = float4(outColor, outAlpha);
}