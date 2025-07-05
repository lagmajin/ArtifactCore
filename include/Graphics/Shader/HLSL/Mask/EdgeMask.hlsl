// SobelEdgeMaskCS.hlsl

Texture2D<float4> inputImage : register(t0);   // 入力画像
RWTexture2D<float> outputMask : register(u0);  // 出力：マスク（白黒）

SamplerState linearSampler : register(s0);     // サンプラ（不要なら省略可）

cbuffer Parameters : register(b0)
{
    int2 textureSize;
    float thresholdMin;
    float thresholdMax;
};

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 uv = DTid.xy;
    if (uv.x >= textureSize.x || uv.y >= textureSize.y)
        return;

    float3 gx = 0;
    float3 gy = 0;

    // 3x3 Sobelフィルタ（グレースケール扱い）
    // カーネル:
    // Gx = [-1 0 1; -2 0 2; -1 0 1]
    // Gy = [-1 -2 -1; 0 0 0; 1 2 1]

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int2 offset = uv + int2(dx, dy);
            // 範囲外アクセスの処理（クランプ）
            offset = clamp(offset, int2(0, 0), textureSize - 1);

            float luminance = dot(inputImage.Load(int3(offset, 0)).rgb, float3(0.299, 0.587, 0.114));

            float kx = 0;
            float ky = 0;

            // Sobel係数（手動展開）
            if (dx == -1 && dy == -1) { kx = -1; ky = -1; }
            if (dx ==  0 && dy == -1) { kx =  0; ky = -2; }
            if (dx ==  1 && dy == -1) { kx =  1; ky = -1; }

            if (dx == -1 && dy ==  0) { kx = -2; ky =  0; }
            if (dx ==  0 && dy ==  0) { kx =  0; ky =  0; }
            if (dx ==  1 && dy ==  0) { kx =  2; ky =  0; }

            if (dx == -1 && dy ==  1) { kx = -1; ky =  1; }
            if (dx ==  0 && dy ==  1) { kx =  0; ky =  2; }
            if (dx ==  1 && dy ==  1) { kx =  1; ky =  1; }

            gx += luminance * kx;
            gy += luminance * ky;
        }
    }

    float magnitude = sqrt(gx * gx + gy * gy); // 画素単位の勾配長
    float mask = smoothstep(thresholdMin, thresholdMax, magnitude);

    outputMask[uv] = mask;  // 書き込み：0.0〜1.0の白黒マスク
}