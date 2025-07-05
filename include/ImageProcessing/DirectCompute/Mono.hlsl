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

    // モノクローム変換を計算 (輝度に基づく)
    // ITU-R BT.709 勧告に基づく加重平均 (一般的な輝度計算)
   float luminance = originalColor.z * 0.2126 +
                  originalColor.y * 0.7152 +
                  originalColor.x * 0.0722;  

    // アルファ成分はそのまま
    float4 monochromeColor = float4(luminance, luminance, luminance, originalColor.a);


OutputTexture[ID.xy] = monochromeColor;
}