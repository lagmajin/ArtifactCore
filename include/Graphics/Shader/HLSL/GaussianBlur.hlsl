Texture2D inputTex : register(t0);
SamplerState samp : register(s0);

cbuffer BlurParams : register(b0)
{
    float2 texelSize;
    int kernelRadius;     // 実際のぼかし半径（<= maxRadius）
    int isHorizontal;     // 1: 横, 0: 縦
};

cbuffer Weights : register(b1)
{
    float weights[11];    // 最大11点分（maxRadius=10）
};

static const int maxRadius = 10;

float4 main(float2 uv : TEXCOORD) : SV_Target
{
    float4 color = float4(0,0,0,0);
    for (int i = -maxRadius; i <= maxRadius; ++i)
    {
        if (abs(i) <= kernelRadius)
        {
            float2 offset = (isHorizontal == 1) ? float2(i, 0) * texelSize : float2(0, i) * texelSize;
            color += inputTex.Sample(samp, uv + offset) * weights[abs(i)];
        }
    }
    return color;
}