Texture2D<float4> InputTexture  : register(t0); // 入力テクスチャ (RGBA順, float4形式)
RWTexture2D<float4> OutputTexture : register(u0); // 出力テクスチャ (RGBA順, float4形式)

// スレッドグループのサイズを定義
[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID) // 全体でのスレッドID
{
    // 入力テクスチャのサイズを取得 (ピクセル単位)
    uint width, height;
    InputTexture.GetDimensions(width, height);

    // スレッドIDがテクスチャの範囲内にあることを確認
    if (ID.x >= width || ID.y >= height)
    {
        return;
    }

    // 入力カラーを読み込む (RGBA)
    float4 originalColor = InputTexture[ID.xy];

    // ネガポジ反転を計算
    // RGB成分を 1.0 から引く
    // アルファ (A) 成分はそのまま
    float4 invertedColor = float4(1.0 - originalColor.r,
                                  1.0 - originalColor.g,
                                  1.0 - originalColor.b,
                                  originalColor.a);

    // 結果を出力テクスチャに書き込む
    OutputTexture[ID.xy] = invertedColor;
}