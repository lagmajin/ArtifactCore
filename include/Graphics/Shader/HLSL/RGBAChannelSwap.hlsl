Texture2D<float4> InputTexture  : register(t0); // 入力テクスチャ
RWTexture2D<float4> OutputTexture : register(u0); // 出力テクスチャ

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);

    if (ID.x >= width || ID.y >= height)
    {
        return;
    }

    float4 originalColor = InputTexture[ID.xy];

    // RとBのチャネルをスワップ
    // 例: originalColor.rgba = (B, G, R, A) の場合、processedColor.rgba = (R, G, B, A) になる
    // 例: originalColor.rgba = (R, G, B, A) の場合、processedColor.rgba = (B, G, R, A) になる
    float4 swappedColor = float4(originalColor.b, originalColor.g, originalColor.r, originalColor.a);

    OutputTexture[ID.xy] = swappedColor;
}