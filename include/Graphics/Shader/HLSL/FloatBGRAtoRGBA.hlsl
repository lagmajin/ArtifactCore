Texture2D<float4> InputBGRA_Texture : register(t0);

// 出力テクスチャ（RGBA順、float4形式）
// テクスチャはアンオーダードアクセスビュー (UAV) としてバインドされる
RWTexture2D<float4> OutputRGBA_Texture : register(u0);

// スレッドグループのサイズを定義
// 例: X軸に8スレッド、Y軸に8スレッド、Z軸に1スレッド
// 画像処理では通常 Z は 1
[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID) // 全体でのスレッドID
{
    // 入力テクスチャのサイズを取得 (ピクセル単位)
    uint width, height;
    InputBGRA_Texture.GetDimensions(width, height);

    // スレッドIDがテクスチャの範囲内にあることを確認
    // 範囲外のスレッドは処理しない
    if (ID.x >= width || ID.y >= height)
    {
        return;
    }

    // BGRAカラーを読み込む
    // color.r は Blue, color.g は Green, color.b は Red, color.a は Alpha
    float4 bgraColor = InputBGRA_Texture[ID.xy];

    // BGRAからRGBAへ並び替える
    // RGBA: Red, Green, Blue, Alpha
    // bgraColor.b は Red
    // bgraColor.g は Green
    // bgraColor.r は Blue
    // bgraColor.a は Alpha
    float4 rgbaColor = float4(bgraColor.b, bgraColor.g, bgraColor.r, bgraColor.a);

    // RGBAカラーを出力テクスチャに書き込む
    OutputRGBA_Texture[ID.xy] = rgbaColor;
}
