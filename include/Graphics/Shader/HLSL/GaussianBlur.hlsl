Texture2D inputTex : register(t0);
SamplerState samp : register(s0);

cbuffer BlurParams : register(b0)
{
    float2 texelSize;
    int kernelRadius;     // ���ۂ̂ڂ������a�i<= maxRadius�j
    int isHorizontal;     // 1: ��, 0: �c
};

cbuffer Weights : register(b1)
{
    float weights[11];    // �ő�11�_���imaxRadius=10�j
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